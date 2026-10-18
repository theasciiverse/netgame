#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/ioctl.h>
using namespace std;

struct tile
{
	int type;
	bool locked;
	bool connected;
};

int sizeX;
int sizeY;

int currentX = 0;
int currentY = 0;

int centreX;
int centreY;

vector<vector<tile> > maze;

map<int,string> tiles;

bool wrapping = false;

const int kMinSize = 2;
const int kMaxSize = 99;
const int kDefaultSize = 11;

void init();
void generateMap();
void setColor(int color);
void setBackColor(int color);
void setCursor(int X, int Y);
void rotateClockwise(int n);
bool findEmptyCell();
int wrapX(int X);
int wrapY(int Y);
void clearConnected();
void setConnected(int X, int Y);
void drawMap();
void drawTile(int X,int Y);
void terminalSetup();
void terminalRestore();
int readKey();
void measure();
bool fits();

//readKey() results beyond plain characters
const int kUp = 256;
const int kDown = 257;
const int kRight = 258;
const int kLeft = 259;
const int kResize = 260;
template<class T> T fromString(const string& s);

int main(int argc, char *argv[])
{
	vector<string> args(argv, argv + argc);
	vector<int> sizes;
	for(size_t i=1; i<args.size(); ++i)
	{
		if(args.at(i) == "--wrapping")
		{
			wrapping = true;
		}
		else if(!args.at(i).empty() && args.at(i).find_first_not_of("0123456789") == string::npos && sizes.size() < 2)
		{
			sizes.push_back(fromString<int>(args.at(i)));
		}
		else
		{
			cout << "usage: " + string(args.at(0)) + " [x-size y-size] [--wrapping]" << endl;
			cout << "Sizes from " << kMinSize << " to " << kMaxSize << " (default " << kDefaultSize << " by " << kDefaultSize << ")" << endl;
			cout << "Move cursor:\t\tArrow keys" << endl;
			cout << "Rotate clockwise:\t. or Space" << endl;
			cout << "Rotate anticlockwise:\t," << endl;
			cout << "Lock tile:\t\tl or s" << endl;
			cout << "Quit:\t\t\tq" << endl;
			return 1;
		}
	}
	//without sizes the default board; a single size makes a square one
	sizeX = sizes.size() > 0 ? sizes.at(0) : kDefaultSize;
	sizeY = sizes.size() > 1 ? sizes.at(1) : sizeX;
	//a 1 by 1 board never finished generating, and 0 crashed
	sizeX = max(kMinSize, min(kMaxSize, sizeX));
	sizeY = max(kMinSize, min(kMaxSize, sizeY));

	centreX = sizeX/2;
	centreY = sizeY/2;

	terminalSetup();

	init();

	//main control loop
	while(1)
	{
		int ch = readKey();
		if(ch == 'q')
		{
			terminalRestore();
			return 0;
		}
		else if(ch == kResize)
		{
			measure();
			cout << "\033[2J";
			drawMap();
		}
		else if(ch == ' ' || ch == '.')
		{
			if(!maze.at(currentX).at(currentY).locked)
			{
				rotateClockwise(1);
				drawMap();
			}
		}
		else if(ch == ',')
		{
			if(!maze.at(currentX).at(currentY).locked)
			{
				rotateClockwise(3);
				drawMap();
			}
		}
		else if(ch == 'l' || ch == 's')
		{
			maze.at(currentX).at(currentY).locked = !maze.at(currentX).at(currentY).locked;
			drawMap();
		}
		else if(ch == kUp || ch == kDown || ch == kLeft || ch == kRight)
		{
			int oldX = currentX;
			int oldY = currentY;
			if(ch == kUp) currentX = wrapX(currentX - 1);
			if(ch == kDown) currentX = wrapX(currentX + 1);
			if(ch == kRight) currentY = wrapY(currentY + 1);
			if(ch == kLeft) currentY = wrapY(currentY - 1);
			if(fits())
			{
				drawTile(oldX, oldY);
				drawTile(currentX, currentY);
				cout << flush;
			}
		}
	}
}

//size of the terminal, and where the board starts in it
int termRows = 24;
int termCols = 80;
int offRow = 0;
int offCol = 0;

const char *kHelp = "arrows move  space or . turn  , turn back  l lock  q quit";

volatile sig_atomic_t resized = 0;

termios before;

void onResize(int)
{
	resized = 1;
}

void terminalSetup()
{
	termios after;
	tcgetattr (STDIN_FILENO, &before); // fill 'before' with current termios values
	after = before; // make a copy to be modified
	after.c_lflag &= (~ICANON); // Disable canonical mode, including line buffering
	after.c_lflag &= (~ECHO); // Don't echo characters on the screen (optional)
	tcsetattr (STDIN_FILENO, TCSANOW, &after); // Set the modified flags

	//no SA_RESTART: a resize interrupts the read() that waits for a key
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = onResize;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGWINCH, &sa, NULL);

	//alternate screen, cursor hidden
	cout << "\033[?1049h\033[?25l";
	measure();
}

void terminalRestore()
{
	cout << "\033[0m\033[?25h\033[?1049l" << flush;
	tcsetattr (STDIN_FILENO, TCSANOW, &before);
}

//reads a key: arrows (ESC [ or ESC O and a letter) come back as kUp and so
//on, a resize as kResize
int readKey()
{
	while(1)
	{
		if(resized)
		{
			resized = 0;
			return kResize;
		}
		unsigned char c;
		ssize_t n = read(STDIN_FILENO, &c, 1);
		if(n == 0)
		{
			return 'q';
		}
		if(n < 0)
		{
			if(errno == EINTR) continue;
			return 'q';
		}
		if(c != 27)
		{
			return c;
		}
		unsigned char seq[2];
		if(read(STDIN_FILENO, &seq[0], 1) != 1 || (seq[0] != '[' && seq[0] != 'O')) continue;
		if(read(STDIN_FILENO, &seq[1], 1) != 1) continue;
		switch(seq[1])
		{
			case 'A': return kUp;
			case 'B': return kDown;
			case 'C': return kRight;
			case 'D': return kLeft;
		}
	}
}

//reads the terminal size and centres the board (two columns per tile) with
//its two lines below
void measure()
{
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0)
	{
		termRows = ws.ws_row;
		termCols = ws.ws_col;
	}
	offRow = max(0, (termRows - (sizeX + 3)) / 2);
	offCol = max(0, (termCols - 2 * sizeY) / 2);
}

//true if the board and its count line fit in the terminal
bool fits()
{
	return termRows >= sizeX + 2 && termCols >= 2 * sizeY;
}

//prints a line centred under the board, cut to the terminal width
void centredLine(int row, const string& text)
{
	string line = text.substr(0, termCols);
	cout << "\033[" << row + 1 << ";1H\033[K";
	cout << "\033[" << row + 1 << ";" << (termCols - (int)line.size()) / 2 + 1 << "H" << line;
}

void init()
{
	//set tile appearances according to binary OR of the directions
	//Up: 0x01
	//Right: 0x02
	//Down: 0x04
	//Left: 0x08
	tiles[0x00] = ".";
	tiles[0x01] = "╨";
	tiles[0x02] = "╞";
	tiles[0x03] = "╚";
	tiles[0x04] = "╥";
	tiles[0x05] = "║";
	tiles[0x06] = "╔";
	tiles[0x07] = "╠";
	tiles[0x08] = "╡";
	tiles[0x09] = "╝";
	tiles[0x0A] = "═";
	tiles[0x0B] = "╩";
	tiles[0x0C] = "╗";
	tiles[0x0D] = "╣";
	tiles[0x0E] = "╦";
	tiles[0x0F] = "╬";

	for(int i=0; i<sizeX; ++i)
	{
		maze.push_back(vector<tile>(sizeY));
	}

	srand(time(NULL));

	generateMap();
	//randomly rotate every tile:
	for(int i=0; i<maze.size(); ++i)
	{
		for(int j=0; j<maze.at(i).size(); ++j)
		{
			currentX = i;
			currentY = j;
			rotateClockwise(rand()%4);
		}
	}
	//clear screen
	cout << "\033[2J";
	drawMap();
}

void generateMap()
{
	//This function needs some serious work
	//Do a series of random walks (without looping) to generate a tree:
	while(findEmptyCell())
	{
		while(1)
		{
			int blocked = 0x00;
			int connected = 0x00;
			bool chosen = false;
			//check which directions are blocked and which directions are connected to the already existing tree
			if((wrapping || currentX > 0) && !maze.at(wrapX(currentX-1)).at(currentY).locked)
			{
				if(maze.at(wrapX(currentX-1)).at(currentY).type) connected |= 0x01;
			}
			else
			{
				blocked |= 0x01;
			}
			if((wrapping || currentY < sizeY-1) && !maze.at(currentX).at(wrapY(currentY+1)).locked)
			{
				if(maze.at(currentX).at(wrapY(currentY+1)).type) connected |= 0x02;
			}
			else
			{
				blocked |= 0x02;
			}
			if((wrapping || currentX < sizeX-1) && !maze.at(wrapX(currentX+1)).at(currentY).locked)
			{
				if(maze.at(wrapX(currentX+1)).at(currentY).type) connected |= 0x04;
			}
			else
			{
				blocked |= 0x04;
			}
			if((wrapping || currentY > 0) && !maze.at(currentX).at(wrapY(currentY-1)).locked)
			{
				if(maze.at(currentX).at(wrapY(currentY-1)).type) connected |= 0x08;
			}
			else
			{
				blocked |= 0x08;
			}
			//if it's possible to connect to the pre-existing tree then do in a random direction
			if(connected)
			{
				while(!chosen)
				{
					switch(rand()%4)
					{
						case 0:
							if(connected & 0x01)
							{
								maze.at(currentX).at(currentY).type |= 0x01;
								currentX = wrapX(currentX - 1);
								maze.at(currentX).at(currentY).type |= 0x04;
								chosen = true;
							}
							break;
						case 1:
							if(connected & 0x02)
							{
								maze.at(currentX).at(currentY).type |= 0x02;
								currentY = wrapY(currentY + 1);
								maze.at(currentX).at(currentY).type |= 0x08;
								chosen = true;
							}
							break;
						case 2:
							if(connected & 0x04)
							{
								maze.at(currentX).at(currentY).type |= 0x04;
								currentX = wrapX(currentX + 1);
								maze.at(currentX).at(currentY).type |= 0x01;
								chosen = true;
							}
							break;
						case 3:
							if(connected & 0x08)
							{
								maze.at(currentX).at(currentY).type |= 0x08;
								currentY = wrapY(currentY - 1);
								maze.at(currentX).at(currentY).type |= 0x02;
								chosen = true;
							}
							break;
					}
				}
				break;
			}
			//if every direction is blocked then break out and start on a new branch (this should only happen with the first random walk)
			else if(blocked == 0x0F)
			{
				break;
			}
			//otherwise go in a random unblocked direction
			else
			{
				while(!chosen)
				{
					switch(rand()%4)
					{
						//if the chosen direction isn't blocked then lock that tile and add the direction we're going
						//also change coordinates to the next tile and add the direction we've come from
						case 0:
							if(!(blocked & 0x01))
							{
								maze.at(currentX).at(currentY).locked = true;
								maze.at(currentX).at(currentY).type |= 1;
								currentX = wrapX(currentX - 1);
								maze.at(currentX).at(currentY).type |= 4;
								chosen = true;
							}
							break;
						case 1:
							if(!(blocked & 0x02))
							{
								maze.at(currentX).at(currentY).locked = true;
								maze.at(currentX).at(currentY).type |= 2;
								currentY = wrapY(currentY + 1);
								maze.at(currentX).at(currentY).type |= 8;
								chosen = true;
							}
							break;
						case 2:
							if(!(blocked & 0x04))
							{
								maze.at(currentX).at(currentY).locked = true;
								maze.at(currentX).at(currentY).type |= 4;
								currentX = wrapX(currentX + 1);
								maze.at(currentX).at(currentY).type |= 1;
								chosen = true;
							}
							break;
						case 3:
							if(!(blocked & 0x08))
							{
								maze.at(currentX).at(currentY).locked = true;
								maze.at(currentX).at(currentY).type |= 8;
								currentY = wrapY(currentY - 1);
								maze.at(currentX).at(currentY).type |= 2;
								chosen = true;
							}
							break;
					}
				}
			}
		}
		//unlock all tiles
		for(int i=0; i<maze.size(); ++i)
		{
			for(int j=0; j<maze.at(i).size(); ++j)
			{
				maze.at(i).at(j).locked = false;
			}
		}
	}
}

void setColor(int color)
{
	if(color == -1)
	{
		cout << "\033[0m";
	}
	else if(color >= 0 && color <8)
	{
		cout << "\033[3" << color << "m";
	}
}

void setBackColor(int color)
{
	if(color == -1)
	{
		cout << "\033[0m";
	}
	else if(color >= 0 && color <8)
	{
		cout << "\033[4" << color << "m";
	}
}

void setCursor(int X, int Y)
{
	cout << "\033[" << offRow+X+1 << ";" << offCol+2*Y+1 << "H";
}

void rotateClockwise(int n)
{
	//use some haxy bit shifting to rotate the tile
	int current = maze.at(currentX).at(currentY).type;
	current &= 0x0F;
	current = (current << n) | (current >> (4 - n));
	current &= 0x0F;
	maze.at(currentX).at(currentY).type &= 0xF0;
	maze.at(currentX).at(currentY).type |= current;
}

bool findEmptyCell()
{
	//find the first empty cell and set current coordinates to that cell
	//used for map generation
	for(int i=0; i<maze.size(); ++i)
	{
		for(int j=0; j<maze.at(i).size(); ++j)
		{
			if(maze.at(i).at(j).type == 0)
			{
				currentX = i;
				currentY = j;
				return true;
			}
		}
	}
	return false;
}

int wrapX(int X)
{
	//not currently used
	//ensure that the coordinates are within our bounds
	return (X%sizeX + sizeX)%sizeX;
}

int wrapY(int Y)
{
	//not currently used
	//ensure that the coordinates are within our bounds
	return (Y%sizeY + sizeY)%sizeY;
}

void clearConnected()
{
	//reset the connection status of all tiles
	for(int i=0; i<maze.size(); ++i)
	{
		for(int j=0; j<maze.at(i).size(); ++j)
		{
			maze.at(i).at(j).connected = false;
		}
	}
}

void setConnected(int X, int Y)
{
	//recursively set the status of each connected tile
	if(!maze.at(X).at(Y).connected)
	{
		maze.at(X).at(Y).connected = true;
		if(maze.at(X).at(Y).type & 0x01)
		{
			if((wrapping || X > 0) && maze.at(wrapX(X-1)).at(Y).type & 0x04)
			{
				setConnected(wrapX(X-1),Y);
			}
		}
		if(maze.at(X).at(Y).type & 0x02)
		{
			if((wrapping || Y < sizeY-1) && maze.at(X).at(wrapY(Y+1)).type & 0x08)
			{
				setConnected(X,wrapY(Y+1));
			}
		}
		if(maze.at(X).at(Y).type & 0x04)
		{
			if((wrapping || X < sizeX-1) && maze.at(wrapX(X+1)).at(Y).type & 0x01)
			{
				setConnected(wrapX(X+1),Y);
			}
		}
		if(maze.at(X).at(Y).type & 0x08)
		{
			if((wrapping || Y > 0) && maze.at(X).at(wrapY(Y-1)).type & 0x02)
			{
				setConnected(X,wrapY(Y-1));
			}
		}
	}
}

void drawMap()
{
	clearConnected();
	setConnected(centreX, centreY);
	if(!fits())
	{
		cout << "\033[0m\033[2J\033[1;1HTerminal too small for this board (q quits)" << flush;
		return;
	}
	int connected = 0;
	for(int i=0; i<maze.size(); ++i)
	{
		for(int j=0; j<maze.at(i).size(); ++j)
		{
			drawTile(i,j);
			if(maze.at(i).at(j).connected)
			{
				++connected;
			}
		}
	}
	//show number connected, and congrats when all pieces are connected
	ostringstream count;
	count << connected << "/" << sizeX*sizeY;
	if(connected == sizeX*sizeY)
	{
		count << "  Congratulations!";
	}
	centredLine(offRow + sizeX + 1, count.str());
	if(termRows >= sizeX + 3)
	{
		centredLine(offRow + sizeX + 2, kHelp);
	}
	cout << flush;
}

void drawTile(int X,int Y)
{
	setCursor(X,Y);
	if(X == centreX && Y == centreY)
	{
		//set centre to purple
		setBackColor(5);
	}
	if(maze.at(X).at(Y).locked)
	{
		//set locked to blue
		setBackColor(4);
	}
	if(maze.at(X).at(Y).connected)
	{
		//set connected green
		setColor(2);
	}
	if(X == currentX && Y == currentY)
	{
		//set cursor to red
		setBackColor(1);
	}
	cout << tiles[maze.at(X).at(Y).type];
	//each tile is two columns wide: the second continues a right arm
	cout << ((maze.at(X).at(Y).type & 0x02) ? "═" : " ");
	setColor(-1);
}

//converts from a string to an arbitray numerical format
template<class T> T fromString(const string& s)
{
	istringstream stream(s);
	T t;
	stream >> t;
	return t;
}
