#include <windows.h>
#include "Structures.h"

// It is called everytime some event happens on the window
// hwnd -> our window, msg -> event, wParam and lParam contains extra info regarding which event is registered
// Examples: wParam holds which key is pressed in case of key, lParam hold coordinates in case of a mouse click
HFONT font;
Editor handler;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: { // Runs the first time when a window is created
        font = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, // Creating a font
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Trebuchet MS");
        handler.tabs[0] = new Tab;
        handler.totalTabs = 1;
        return 0;
    }
    case WM_PAINT: { // Runs everytime window needs to be drawn
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Making the entire window and making it a specific colour
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 40)); // Setting colour
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush); // Finishing colouring by disabling it

        SetBkMode(hdc, TRANSPARENT); // Setting text's background to transparent
        SetTextColor(hdc, RGB(255, 255, 255)); // Setting text's colour to white

        SelectObject(hdc, font); // Activating the use of hdc font

        // Drawing the buffer

        // Creates a layout to store organized buffer
        Wrapper wrap = buildLayout(handler.tabs[handler.currentTab]->buffer, handler.tabs[handler.currentTab]->length, handler.tabs[handler.currentTab]->linesPerCol, handler.tabs[handler.currentTab]->charsPerLine, handler.tabs[handler.currentTab]->colsPerPage);
        // Calculates the number of line which is the first and last line of the concerned page
        int firstLine = handler.tabs[handler.currentTab]->page * handler.tabs[handler.currentTab]->linesPerCol * handler.tabs[handler.currentTab]->colsPerPage;
        int lastLine = min(firstLine + (handler.tabs[handler.currentTab]->linesPerCol * handler.tabs[handler.currentTab]->colsPerPage) - 1, wrap.totalLines - 1);
        // Outputs every line sequentially
        for (int i=firstLine; i<lastLine; i++)
            TextOutA(hdc, 50, (i - firstLine) * 28, handler.tabs[handler.currentTab]->buffer + wrap.array[i].start, wrap.array[i].size);
        deleteLayout(wrap); // Deletes the layout
        EndPaint(hwnd, &ps); // Tells Windows that painting is finished
        return 0;
    }
    case WM_CHAR: { // Runs when a key is pressed which produces a character
        switch (wParam) {
        case VK_BACK:
            backSpaceCharacter(*handler.tabs[handler.currentTab]);
            break;
        case '\r':
            insertCharacter(*handler.tabs[handler.currentTab], '\n');
            break;
        default:
            insertCharacter(*handler.tabs[handler.currentTab], wParam);
        }
        InvalidateRect(hwnd, NULL, FALSE); // Redrawing the window
        return 0;
    }
    case WM_KEYDOWN: { // Runs when a key is pressed which doesn't produce a character
        bool shift = GetKeyState(VK_SHIFT) & 0x8000;
        switch (wParam) {
        case VK_DELETE:
            deleteCharacter(*handler.tabs[handler.currentTab]);
            break;
        case VK_HOME:
            home(*handler.tabs[handler.currentTab], shift);
            break;
        case VK_END:
            end(*handler.tabs[handler.currentTab], shift);
            break;
        case VK_UP:
            upArrow(*handler.tabs[handler.currentTab], shift);
            break;
        case VK_DOWN:
            downArrow(*handler.tabs[handler.currentTab], shift);
            break;
        case VK_RIGHT:
            rightArrow(*handler.tabs[handler.currentTab], shift);
            break;
        case VK_LEFT:
            leftArrow(*handler.tabs[handler.currentTab], shift);
            break;
        }
        InvalidateRect(hwnd, NULL, FALSE); // Redrawing the window
        return 0;
    }
    case WM_DESTROY: { // Runs when user clicks the 'X' to close the window
        PostQuitMessage(0);
        handler.~Editor();
        DeleteObject(font); // Frees the font
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam); // Used for the events we do not explicitly handle, so that windows my apply its default behaviour
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"FastianWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc; // Which function to call when something happens on the window
    wc.hInstance = hInstance; // Unique ID of your window
    wc.lpszClassName = CLASS_NAME; // Assigning a name to your window object
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Background colour
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); // Cursor looks

    RegisterClassW(&wc); // Makes above window object official by registering it with Windows

    // If hwnd is returned than everything is right, else if NULL is returned then something went wrong
    HWND hwnd = CreateWindowExW( // Actual creation of window happens here
        0,
        CLASS_NAME,
        L"The Aracanum Editor", // Name appearing in the title bar
        WS_OVERLAPPEDWINDOW, // Basic window tempelate (title bar, buttons of top right)
        CW_USEDEFAULT, CW_USEDEFAULT, 1440, 720, // Initial dimensions of the window
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow); // Responsible to make the window appear for the first time
    UpdateWindow(hwnd); // Responsibel to draw the window by calling WM_PAINT

    MSG msg = {};
    // Following loop runs forever, until a program is closed
    while (GetMessage(&msg, NULL, 0, 0)) { // Receives an event and store it in msg, stops when WM_QUIT is received
        TranslateMessage(&msg); // Converts event to WM_CHAR (raw bytes to an actual character)
        DispatchMessage(&msg); // Sends this WM_CHAR to WndProc
    }

    return 0;
}


// Definitions of the functions of Structures.h


// Used to format the whole input into lines
Wrapper buildLayout(char* buffer, int length, int linesPerCol, int charsPerLine, int colsPerPage) {
    Wrapper finalArray{ 0, 0, new Layout[length + 1] };
    int start = 0;
    for (int i = 0, j = 0; i < length; i++) {
        Layout currentLine;
        if (*(buffer + i) == ' ')
            j = i;
        if (i - start == charsPerLine) {
            if (*(buffer + i) == '\n') {
                currentLine.start = start;
                currentLine.end = i;
                currentLine.size = i - start;
                currentLine.endedPeacefully = true;
                start = i + 1;
                j = start;
            }
            else {
                currentLine.start = start;
                currentLine.end = j;
                currentLine.size = i - start;
                currentLine.endedPeacefully = false;
                start = j + 1;
                j = start;
            }
            *(finalArray.array + finalArray.totalLines) = currentLine;
            finalArray.totalLines++;
        }
        else if (*(buffer + i) == '\n') {
            currentLine.start = start;
            currentLine.end = i;
            currentLine.size = i - start;
            currentLine.endedPeacefully = true;
            start = i + 1;
            j = start;
            *(finalArray.array + finalArray.totalLines) = currentLine;
            finalArray.totalLines++;
        }
    }
    Layout lastActiveLine;
    if (start < length)
        lastActiveLine = { start, length, length - start, true };
    else
        lastActiveLine = { length, length, length, true };
    *(finalArray.array + finalArray.totalLines) = lastActiveLine;
    finalArray.totalLines++;
    int linesPerPage = linesPerCol * colsPerPage;
    finalArray.totalPages = finalArray.totalLines / linesPerPage;
    finalArray.totalPages += (finalArray.totalPages == (finalArray.totalLines / static_cast<double>(linesPerPage)) ? 0 : 1);
    return finalArray;
}

// Used to clean up memory acquired by Wrapper's pointer (array)
void deleteLayout(Wrapper layout) {
    delete[] layout.array;
}

// Following functions take care of the typing procedure

// Typing a new character
void insertCharacter(Tab& obj, char ch) {
    if (obj.length == obj.capacity) {
        obj.capacity *= 2;
        char* ptr = new char[obj.capacity];
        for (int i = 0; i < obj.length; i++)
            *(ptr + i) = *(obj.buffer + i);
        delete[] obj.buffer;
        obj.buffer = ptr;
    }
    for (int i = obj.length; i > obj.cursor; i--) {
        char temp = *(obj.buffer + i);
        *(obj.buffer + i) = *(obj.buffer + i - 1);
        *(obj.buffer + i - 1) = temp;
    }
    obj.length++;
    *(obj.buffer + obj.cursor) = ch;
    obj.cursor++;
    return;
}

// Deleting a range
void deleteRange(Tab& obj, int start, int end) {
    int s = start, e = end;
    for (; end < obj.length; start++, end++)
        *(obj.buffer + start) = *(obj.buffer + end);
    for (; start < obj.length; start++)
        *(obj.buffer + start) = '\0';
    obj.length -= (e - s);
    obj.cursor = ((obj.cursor > s) && (obj.cursor < e)) ? s : obj.cursor;
    return;
}

// Delete a character (with backspace)
void backSpaceCharacter(Tab& obj) {
    if (obj.beingSelected)
        deleteRange(obj, ((obj.anchor > obj.cursor) ? obj.cursor : obj.anchor), ((obj.anchor > obj.cursor) ? obj.anchor : obj.cursor));
    else if (obj.cursor != 0) {
        deleteRange(obj, obj.cursor - 1, obj.cursor);
        obj.cursor--;
    }
    return;
}

// Delete a character (with delete)
void deleteCharacter(Tab& obj) {
    if (obj.beingSelected)
        deleteRange(obj, ((obj.anchor > obj.cursor) ? obj.cursor : obj.anchor), ((obj.anchor > obj.cursor) ? obj.anchor : obj.cursor));
    else if (obj.cursor != obj.length)
        deleteRange(obj, obj.cursor, obj.cursor + 1);
    return;
}

// Movement of cursor, subject to arrow keys

// Left key handling
void leftArrow(Tab& obj, bool shiftHeld) {
    if (shiftHeld) {
        obj.anchor = (obj.anchor == -1) ? obj.cursor : obj.anchor;
        obj.beingSelected = true;
    }
    else {
        obj.beingSelected = false;
        obj.anchor = -1;
    }
    if (obj.cursor != 0)
        obj.cursor--;
}

// Right key handling
void rightArrow(Tab& obj, bool shiftHeld) {
    if (shiftHeld) {
        obj.anchor = (obj.anchor == -1) ? obj.cursor : obj.anchor;
        obj.beingSelected = true;
    }
    else {
        obj.beingSelected = false;
        obj.anchor = -1;
    }
    if (obj.cursor != obj.length)
        obj.cursor++;
}

// Helper function to find the number of line curretly active
int currentLine(Tab& obj, Wrapper input) {
    for (int i = 0; i < input.totalLines; i++)
        if ((obj.cursor >= input.array[i].start) && (obj.cursor <= input.array[i].end))
            return i;
    return 0;
}

// Up key handling
void upArrow(Tab& obj, bool shiftHeld) {
    Wrapper input = buildLayout(obj.buffer, obj.length, obj.linesPerCol, obj.charsPerLine, obj.colsPerPage);
    int line = currentLine(obj, input);
    if (shiftHeld) {
        obj.anchor = (obj.anchor == -1) ? obj.cursor : obj.anchor;
        obj.beingSelected = true;
    }
    else {
        obj.beingSelected = false;
        obj.anchor = -1;
    }
    if (line != 0) {
        int temp = obj.cursor - input.array[line].start;
        if ((input.array[line - 1].start + temp) < input.array[line - 1].end)
            obj.cursor = input.array[line - 1].start + temp;
        else
            obj.cursor = input.array[line - 1].end;
    }
    else
        obj.cursor = 0;
    deleteLayout(input);
}

// Down key handling
void downArrow(Tab& obj, bool shiftHeld) {
    Wrapper input = buildLayout(obj.buffer, obj.length, obj.linesPerCol, obj.charsPerLine, obj.colsPerPage);
    int line = currentLine(obj, input);
    if (shiftHeld) {
        obj.anchor = (obj.anchor == -1) ? obj.cursor : obj.anchor;
        obj.beingSelected = true;
    }
    else {
        obj.beingSelected = false;
        obj.anchor = -1;
    }
    if (line != input.totalLines - 1) {
        int temp = obj.cursor - input.array[line].start;
        if ((input.array[line + 1].start + temp) < input.array[line + 1].end)
            obj.cursor = input.array[line + 1].start + temp;
        else
            obj.cursor = input.array[line + 1].end;
    }
    else
        obj.cursor = obj.length;
    deleteLayout(input);
}

// Home key handling
void home(Tab& obj, bool shiftHeld) {
    Wrapper input = buildLayout(obj.buffer, obj.length, obj.linesPerCol, obj.charsPerLine, obj.colsPerPage);
    if (shiftHeld) {
        obj.anchor = (obj.anchor == -1) ? obj.cursor : obj.anchor;
        obj.beingSelected = true;
    }
    else {
        obj.beingSelected = false;
        obj.anchor = -1;
    }
    obj.cursor = input.array[currentLine(obj, input)].start;
    deleteLayout(input);
}

// End key handling
void end(Tab& obj, bool shiftHeld) {
    Wrapper input = buildLayout(obj.buffer, obj.length, obj.linesPerCol, obj.charsPerLine, obj.colsPerPage);
    if (shiftHeld) {
        obj.anchor = (obj.anchor == -1) ? obj.cursor : obj.anchor;
        obj.beingSelected = true;
    }
    else {
        obj.beingSelected = false;
        obj.anchor = -1;
    }
    obj.cursor = input.array[currentLine(obj, input)].end;
    deleteLayout(input);
}