#pragma once
#ifndef STRUCTURES_H
#define STRUCTURES_H

// Checking commit

// Defines general form of a Table of Content entry
struct TOC {
	char title[128]; // Title in Table of Content
	int pageNumber; // Page number of the corresponding title
};

// Represents the current tab
class Tab {
public:
	int length, capacity, cursor, anchor, page, align, linesPerCol, charsPerLine, colsPerPage, saves, tocCount; // length -> Size of current input, capacity -> Max size of buffer, anchor -> Point of start of a selection, tocCount -> How many TOCs are there yet
	bool beingSelected, dirty; // beingSelected -> Represents whether text is being selected, dirty -> Reperesents if there has been a change after the latest save
	char* buffer, path[128]; // buffer -> Place where input is stored, path -> Place where path of a new opening file is stored
	TOC arr[256]; // -> Array of table of contents (max TOC in a document = 256)

	Tab(int x = 20, int y = 40, int z = 2) {
		beingSelected = dirty = false;
		page = cursor = saves = tocCount = length = 0;
		anchor = -1;
		capacity = 1024; // Initial memory for the buffer
		align = 1; // Left
		linesPerCol = x;
		charsPerLine = y;
		colsPerPage = z;
		buffer = new char[capacity];
		for (int i = 0; i < capacity; i++)
			*(buffer + i) = '\0';
	}
	~Tab() { delete[] buffer; }
	void left() { align = 1; } // Left Align
	void right() { align = 2; } // Right Align
	void center() { align = 3; } // Center Align
	void justify() { align = 4; } // Justify
};

// Defines general form of tracking the history of searches
struct SearchHistory {
	char search[128]; // Word that has been searched
	int matches; // Number of matches of that word in the document
};

// Represents the entire application itself
class Editor {
public:
	Tab* tabs[10]; // Number of tabs open (max = 10)
	SearchHistory searches[5]; // Record of searches (max = 5)
	int currentTab, totalTabs, searchCount, dictionaryCount; // searchCount -> Number of searches done, dictionaryCount -> Number of words in the dictionary
	char searched[128], searching[128]; // Searched -> after pressing enter, Searching -> while typing
	bool isSearching, found, mouseClicked; // found -> There's a match for a search, mouseClicked -> Used to differentiate b/w click and drag
	char** dictionary; // Points to the dictionary

	Editor(int x = 0) { // Default value for the dictionary (if not included)
		currentTab = totalTabs = 0; // Tabs start at zero
		dictionaryCount = x;
		isSearching = found = mouseClicked = false;
	}
};

// Represents the formatting of a single line
struct Layout {
	int start, end, size; // Start and End and Size (charsPerLine) of a particular line from the buffer
	bool endedPeacefully; // True -> Line ends due to '\n', False -> Line ends because character overflow
};

// Represents the final formatted version of the buffer
struct Wrapper {
	int totalLines, totalPages; // Represents the total lines and pages a document has, respectfully
	Layout* array; // An array of Layouts (lines)
};

// Used to format the whole input into lines
Wrapper buildLayout(char* buffer, int length, int linesPerCol, int charsPerLine, int colsPerPage);

// Used to clean up memory acquired by Wrapper's pointer (array)
void deleteLayout(Wrapper layout);

// Following functions take care of the typing procedure

// Typing a new character
void insertCharacter(Tab& obj, char ch);

// Deleting a range
void deleteRange(Tab& obj, int start, int end);

// Delete a character (with backspace)
void backSpaceCharacter(Tab& obj);

// Delete a character (with delete)
void deleteCharacter(Tab& obj);

// Movement of cursor, subject to arrow keys

// Left key handling
void leftArrow(Tab& obj, bool shiftHeld);

// Right key handling
void rightArrow(Tab& obj, bool shiftHeld);

// Helper function to find the number of line curretly active
int currentLine(Tab& obj, Wrapper layout);

// Up key handling
void upArrow(Tab& obj, bool shiftHeld);

// Down key handling
void downArrow(Tab& obj, bool shiftHeld);

// Home key handling
void home(Tab& obj, bool shiftHeld);

// End key handling
void end(Tab& obj, bool shiftHeld);

#endif