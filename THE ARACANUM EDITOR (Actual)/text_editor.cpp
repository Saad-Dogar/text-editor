#include <windows.h>
#include <fstream>
#include "structure.h"

// It is called everytime some event happens on the window
// hwnd -> our window, msg -> event, wParam and lParam contains extra info regarding which event is registered
// Examples: wParam holds which key is pressed in case of key, lParam hold coordinates in case of a mouse click
HFONT font;
HFONT smallFont;
Editor handler;
int fontHeight = 0, fontWidth = 0;
bool caretVisible = true;
bool showSidePanel = true;
bool showHelpOverlay = false;

void drawHLine(HDC hdc, int x1, int x2, int y, COLORREF colour);
void drawRect(HDC hdc, int x1, int y1, int x2, int y2, COLORREF colour);


int promptStage = 0;
wchar_t promptBuffer[32] = {}; // Holds what the user is typing into the current prompt field
int promptLen = 0;             // Number of characters typed so far in the prompt

// Layout constants for the UI chrome
const int MARGIN_X = 60;  // Left margin before first column
const int MARGIN_Y = 40;  // Top margin above columns
const int COL_GAP = 50;  // Gap between columns (divider lives in the middle of this)
const int STATUS_H = 28;  // Height of the status bar at the bottom
const int FOOTER_H = 24;  // Height of the page footer above the status bar

// Colours used throughout the UI
const COLORREF CLR_BACKGROUND = RGB(18, 18, 36);
const COLORREF CLR_PAPER = RGB(28, 28, 52);
const COLORREF CLR_TEXT = RGB(220, 220, 235);
const COLORREF CLR_DIVIDER = RGB(70, 70, 110);
const COLORREF CLR_STATUS_BG = RGB(10, 10, 25);
const COLORREF CLR_STATUS_TEXT = RGB(140, 140, 180);
const COLORREF CLR_LABEL = RGB(80, 80, 120);
const COLORREF CLR_FOOTER = RGB(100, 100, 150);
const COLORREF CLR_HIGHLIGHT = RGB(70, 90, 160);
const COLORREF CLR_ACCENT = RGB(120, 100, 200);
const COLORREF CLR_PROMPT_BG = RGB(22, 22, 45);
const COLORREF CLR_PROMPT_BOX = RGB(40, 40, 75);
const COLORREF CLR_FIND_HIGHLIGHT = RGB(120, 100, 40);

const UINT MENU_CUT = 2001;
const UINT MENU_COPY = 2002;
const UINT MENU_PASTE = 2003;
const UINT MENU_FIND = 2004;
const UINT MENU_ALIGN_LEFT = 2005;
const UINT MENU_ALIGN_RIGHT = 2006;
const UINT MENU_ALIGN_CENTER = 2007;
const UINT MENU_ALIGN_JUSTIFY = 2008;

// Returns a short string for the current alignment mode
const wchar_t* alignName(int align) {
    switch (align) {
    case 1: return L"Left";
    case 2: return L"Right";
    case 3: return L"Center";
    case 4: return L"Justify";
    }
    return L"Left";
}

int findMatch(char* buffer, int length, const char* term, int from, bool forward) {
    int n = 0;
    while (term[n] != '\0') n++;
    if (n == 0 || length < n)
        return -1;

    if (forward) {
        if (from < 0) from = 0;
        if (from > length - n) return -1;
        for (int i = from; i <= length - n; i++) {
            bool ok = true;
            for (int j = 0; j < n; j++) {
                char a = buffer[i + j];
                char b = term[j];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                if (a != b) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return i;
        }
    }
    else {
        if (from > length - n) from = length - n;
        if (from < 0) return -1;
        for (int i = from; i >= 0; i--) {
            bool ok = true;
            for (int j = 0; j < n; j++) {
                char a = buffer[i + j];
                char b = term[j];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                if (a != b) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return i;
        }
    }

    return -1;
}

bool sameIgnoreCase(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return false;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

void jumpToSearchMatch(bool forward) {
    if (handler.searched[0] == '\0')
        return;

    Tab* tab = handler.tabs[handler.currentTab];
    int n = 0;
    while (handler.searched[n] != '\0') n++;
    if (n == 0 || tab->length < n)
        return;

    int idx = -1;
    if (forward) {
        idx = findMatch(tab->buffer, tab->length, handler.searched, tab->cursor + 1, true);
        if (idx == -1)
            idx = findMatch(tab->buffer, tab->length, handler.searched, 0, true);
    }
    else {
        idx = findMatch(tab->buffer, tab->length, handler.searched, tab->cursor - 1, false);
        if (idx == -1)
            idx = findMatch(tab->buffer, tab->length, handler.searched, tab->length - n, false);
    }

    if (idx != -1) {
        tab->cursor = idx;
        tab->anchor = -1;
        tab->beingSelected = false;

        Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);
        int cursorLineIdx = currentLine(*tab, wrap);
        int linesPerPage = tab->linesPerCol * tab->colsPerPage;
        tab->page = cursorLineIdx / linesPerPage;
        deleteLayout(wrap);
    }
}

void syncPageToCursor(Tab* tab) {
    Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);
    int cursorLineIdx = currentLine(*tab, wrap);
    int linesPerPage = tab->linesPerCol * tab->colsPerPage;
    tab->page = cursorLineIdx / linesPerPage;
    deleteLayout(wrap);
}

bool copySelectionToClipboard(HWND hwnd, Tab* tab) {
    if (!(tab->beingSelected && tab->anchor != -1 && tab->anchor != tab->cursor))
        return false;

    int start = (tab->anchor < tab->cursor) ? tab->anchor : tab->cursor;
    int end = (tab->anchor > tab->cursor) ? tab->anchor : tab->cursor;
    int len = end - start;

    if (!OpenClipboard(hwnd))
        return false;

    EmptyClipboard();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (mem) {
        char* dst = (char*)GlobalLock(mem);
        if (dst) {
            for (int i = 0; i < len; i++)
                dst[i] = tab->buffer[start + i];
            dst[len] = '\0';
            GlobalUnlock(mem);
            SetClipboardData(CF_TEXT, mem);
        }
    }
    CloseClipboard();
    return true;
}

bool cutSelectionToClipboard(HWND hwnd, Tab* tab) {
    if (!copySelectionToClipboard(hwnd, tab))
        return false;

    int start = (tab->anchor < tab->cursor) ? tab->anchor : tab->cursor;
    int end = (tab->anchor > tab->cursor) ? tab->anchor : tab->cursor;
    deleteRange(*tab, start, end);
    tab->cursor = start;
    tab->beingSelected = false;
    tab->dirty = true;
    syncPageToCursor(tab);
    return true;
}

bool pasteFromClipboard(HWND hwnd, Tab* tab) {
    bool changed = false;
    if (OpenClipboard(hwnd)) {
        HANDLE data = GetClipboardData(CF_TEXT);
        if (data) {
            char* src = (char*)GlobalLock(data);
            if (src) {
                int pasteLen = 0;
                while (src[pasteLen] != '\0')
                    pasteLen++;

                if (tab->beingSelected && tab->anchor != -1 && tab->anchor != tab->cursor) {
                    int start = (tab->anchor < tab->cursor) ? tab->anchor : tab->cursor;
                    int end = (tab->anchor > tab->cursor) ? tab->anchor : tab->cursor;
                    deleteRange(*tab, start, end);
                    tab->cursor = start;
                    tab->beingSelected = false;
                }

                while (tab->capacity <= tab->length + pasteLen + 1)
                    tab->capacity *= 2;

                char* grown = new char[tab->capacity];
                for (int i = 0; i < tab->capacity; i++)
                    grown[i] = '\0';
                for (int i = 0; i < tab->length; i++)
                    grown[i] = tab->buffer[i];
                delete[] tab->buffer;
                tab->buffer = grown;

                for (int i = tab->length - 1; i >= tab->cursor; i--)
                    tab->buffer[i + pasteLen] = tab->buffer[i];
                for (int i = 0; i < pasteLen; i++)
                    tab->buffer[tab->cursor + i] = src[i];

                tab->length += pasteLen;
                tab->cursor += pasteLen;
                tab->anchor = -1;
                tab->beingSelected = false;

                tab->dirty = true;
                syncPageToCursor(tab);
                changed = true;

                GlobalUnlock(data);
            }
        }
        CloseClipboard();
    }
    return changed;
}

int appendWide(wchar_t* dest, int at, const wchar_t* src, int cap) {
    for (int i = at; i < cap - 1 && *src != L'\0'; i++, src++) {
        dest[i] = *src;
        at = i + 1;
    }
    if (at < cap)
        dest[at] = L'\0';
    return at;
}

int appendInt(wchar_t* dest, int at, int value, int cap) {
    if (value == 0) {
        if (at < cap - 1) {
            dest[at++] = L'0';
            dest[at] = L'\0';
        }
        return at;
    }

    if (value < 0) {
        if (at < cap - 1)
            dest[at++] = L'-';
        value = -value;
    }

    wchar_t temp[16] = {};
    int len = 0;
    while (value > 0 && len < 15) {
        temp[len++] = (wchar_t)(L'0' + (value % 10));
        value /= 10;
    }

    for (int i = len - 1; i >= 0; i--) {
        if (at < cap - 1)
            dest[at++] = temp[i];
    }
    if (at < cap)
        dest[at] = L'\0';
    return at;
}

void tabFilePath(int tabIdx, wchar_t* path, int cap) {
    int at = 0;
    path[0] = L'\0';
    at = appendWide(path, at, L"Tab_", cap);
    at = appendInt(path, at, tabIdx + 1, cap);
    appendWide(path, at, L".txt", cap);
}

void tabAutoPath(int tabIdx, int copyIdx, wchar_t* path, int cap) {
    int at = 0;
    path[0] = L'\0';
    at = appendWide(path, at, L"Tab_", cap);
    at = appendInt(path, at, tabIdx + 1, cap);
    at = appendWide(path, at, L"_auto_", cap);
    at = appendInt(path, at, copyIdx, cap);
    appendWide(path, at, L".txt", cap);
}

void saveTabToPath(Tab* tab, const wchar_t* path) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, tab->buffer, (DWORD)tab->length, &written, NULL);
        CloseHandle(file);
        tab->dirty = false;
    }
}

void loadTabFromPath(Tab* tab, const wchar_t* path) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD fileSize = GetFileSize(file, NULL);
        if (fileSize != INVALID_FILE_SIZE) {
            while (tab->capacity <= (int)fileSize + 1)
                tab->capacity *= 2;

            char* newBuffer = new char[tab->capacity];
            for (int i = 0; i < tab->capacity; i++)
                newBuffer[i] = '\0';
            delete[] tab->buffer;
            tab->buffer = newBuffer;

            DWORD read = 0;
            if (ReadFile(file, tab->buffer, fileSize, &read, NULL)) {
                tab->length = (int)read;
                tab->cursor = 0;
                tab->anchor = -1;
                tab->page = 0;
                tab->dirty = false;
            }
        }
        CloseHandle(file);
    }
}

void autoSaveCurrentTab() {
    int tabIdx = handler.currentTab;
    Tab* tab = handler.tabs[tabIdx];
    bool wasDirty = tab->dirty;

    for (int i = 9; i >= 1; i--) {
        wchar_t src[64] = {}, dst[64] = {};
        tabAutoPath(tabIdx, i - 1, src, 64);
        tabAutoPath(tabIdx, i, dst, 64);
        DeleteFileW(dst);
        MoveFileW(src, dst);
    }

    wchar_t newest[64] = {};
    tabAutoPath(tabIdx, 0, newest, 64);
    saveTabToPath(tab, newest);
    tab->dirty = wasDirty;
}

int narrowLen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void copyNarrow(char* dst, const char* src, int cap) {
    int i = 0;
    for (; i < cap - 1 && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

void storeSearch(const char* term, int matches) {
    int foundAt = -1;
    for (int i = 0; i < handler.searchCount; i++) {
        if (sameIgnoreCase(handler.searches[i].search, term)) {
            foundAt = i;
            break;
        }
    }

    if (foundAt != -1) {
        for (int i = foundAt + 1; i < handler.searchCount; i++) {
            copyNarrow(handler.searches[i - 1].search, handler.searches[i].search, 128);
            handler.searches[i - 1].matches = handler.searches[i].matches;
        }
        copyNarrow(handler.searches[handler.searchCount - 1].search, term, 128);
        handler.searches[handler.searchCount - 1].matches = matches;
        return;
    }

    if (handler.searchCount < 5) {
        copyNarrow(handler.searches[handler.searchCount].search, term, 128);
        handler.searches[handler.searchCount].matches = matches;
        handler.searchCount++;
        return;
    }

    for (int i = 1; i < 5; i++) {
        copyNarrow(handler.searches[i - 1].search, handler.searches[i].search, 128);
        handler.searches[i - 1].matches = handler.searches[i].matches;
    }
    copyNarrow(handler.searches[4].search, term, 128);
    handler.searches[4].matches = matches;
}

int countMatches(char* buffer, int length, const char* term) {
    int n = narrowLen(term);
    if (n == 0) return 0;

    int total = 0;
    for (int i = 0; i <= length - n; i++) {
        bool ok = true;
        for (int j = 0; j < n; j++) {
            char a = buffer[i + j];
            char b = term[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) {
                ok = false;
                break;
            }
        }
        if (ok) {
            total++;
            i += n - 1;
        }
    }
    return total;
}

void drawSearchBar(HDC hdc, RECT& clientRect) {
    if (!handler.isSearching && handler.searched[0] == '\0')
        return;

    int y1 = clientRect.bottom - STATUS_H - 24;
    int y2 = clientRect.bottom - STATUS_H;
    RECT bar = { 0, y1, clientRect.right, y2 };

    HBRUSH b = CreateSolidBrush(CLR_PROMPT_BG);
    FillRect(hdc, &bar, b);
    DeleteObject(b);
    drawHLine(hdc, 0, clientRect.right, y1, CLR_DIVIDER);

    wchar_t line[256] = {};
    int at = 0;
    at = appendWide(line, at, L"Find: ", 256);

    const char* src = handler.isSearching ? handler.searching : handler.searched;
    for (int i = 0; src[i] != '\0' && at < 255; i++)
        line[at++] = (wchar_t)(unsigned char)src[i];
    line[at] = L'\0';

    if (!handler.isSearching) {
        at = appendWide(line, at, L"   Matches: ", 256);
        int m = (handler.searchCount > 0) ? handler.searches[handler.searchCount - 1].matches : 0;
        appendInt(line, at, m, 256);
    }

    SelectObject(hdc, smallFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);
    TextOutW(hdc, 10, y1 + 5, line, (int)wcslen(line));
}

void drawHelpOverlay(HDC hdc, RECT& clientRect) {
    int w = 520, h = 360;
    int x = (clientRect.right - w) / 2;
    int y = (clientRect.bottom - h) / 2;

    RECT panel = { x, y, x + w, y + h };
    HBRUSH b = CreateSolidBrush(CLR_PROMPT_BG);
    FillRect(hdc, &panel, b);
    DeleteObject(b);
    drawRect(hdc, x, y, x + w, y + h, CLR_ACCENT);

    SelectObject(hdc, smallFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_ACCENT);

    const wchar_t* title = L"HELP / SHORTCUTS";
    TextOutW(hdc, x + 18, y + 14, title, (int)wcslen(title));
    drawHLine(hdc, x + 10, x + w - 10, y + 38, CLR_DIVIDER);

    SetTextColor(hdc, CLR_TEXT);
    int ty = y + 56;
    const wchar_t* lines[] = {
        L"Ctrl + N         New tab",
        L"Ctrl + W         Close current tab",
        L"Ctrl + S         Save current tab",
        L"Ctrl + Shift + S Save all tabs",
        L"Ctrl + O         Open current tab file",
        L"Ctrl + F         Find and highlight",
        L"> / <            Next / Previous match",
        L"Ctrl + Tab       Next tab",
        L"Ctrl + Shift+Tab Prev tab",
        L"Ctrl + L/R/E/J   Left/Right/Center/Justify",
        L"Ctrl + P         Toggle right panel",
        L"F1               Toggle this help overlay"
    };

    for (int i = 0; i < 12; i++) {
        TextOutW(hdc, x + 18, ty, lines[i], (int)wcslen(lines[i]));
        ty += 24;
    }
}

// Gets full stats in a buffer of given length
void getStats(char* buffer, int length, int& words, int& charsWith, int& charsWithout, int& sentences) {
    words = sentences = charsWith = charsWithout = 0;
    bool inWord = false;
    for (int i = 0; i < length; i++) {
        char c = *(buffer + i);
        charsWith++;
        if (c != ' ' && c != '\n' && c != '\t') {
            charsWithout++;
        }
        if (c == ' ' || c == '\n' || c == '\t') {
            inWord = false;
        }
        else {
            if (!inWord) words++;
            inWord = true;
        }
        if (c == '.' || c == '?' || c == '!') {
            sentences++;
        }
    }
}

// Draws a 1 pixel thick horizontal line using FillRect
void drawHLine(HDC hdc, int x1, int x2, int y, COLORREF colour) {
    RECT r = { x1, y, x2, y + 1 };
    HBRUSH br = CreateSolidBrush(colour);
    FillRect(hdc, &r, br);
    DeleteObject(br);
}

// Draws a 1 pixel thick vertical line using FillRect
void drawVLine(HDC hdc, int x, int y1, int y2, COLORREF colour) {
    RECT r = { x, y1, x + 1, y2 };
    HBRUSH br = CreateSolidBrush(colour);
    FillRect(hdc, &r, br);
    DeleteObject(br);
}

// Draws a hollow rectangle border
void drawRect(HDC hdc, int x1, int y1, int x2, int y2, COLORREF colour) {
    drawHLine(hdc, x1, x2, y1, colour); // Top
    drawHLine(hdc, x1, x2, y2 - 1, colour); // Bottom
    drawVLine(hdc, x1, y1, y2, colour); // Left
    drawVLine(hdc, x2 - 1, y1, y2, colour); // Right
}

// Draws the startup prompt screen that collects linesPerCol, charsPerLine, colsPerPage
void drawPromptScreen(HDC hdc, RECT& clientRect) {
    // Full background
    HBRUSH bgBrush = CreateSolidBrush(CLR_BACKGROUND);
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Centred panel
    int panelW = 420, panelH = 220;
    int panelX = (clientRect.right - panelW) / 2;
    int panelY = (clientRect.bottom - panelH) / 2;

    RECT panel = { panelX, panelY, panelX + panelW, panelY + panelH };
    HBRUSH panelBrush = CreateSolidBrush(CLR_PROMPT_BG);
    FillRect(hdc, &panel, panelBrush);
    DeleteObject(panelBrush);
    drawRect(hdc, panelX, panelY, panelX + panelW, panelY + panelH, CLR_ACCENT);

    SelectObject(hdc, smallFont);
    SetBkMode(hdc, TRANSPARENT);

    // Title
    const wchar_t* title = L"THE ARACANUM EDITOR  -  Setup";
    SIZE titleSz;
    GetTextExtentPoint32W(hdc, title, (int)wcslen(title), &titleSz);
    SetTextColor(hdc, CLR_ACCENT);
    TextOutW(hdc, panelX + (panelW - titleSz.cx) / 2, panelY + 14, title, (int)wcslen(title));

    // Horizontal divider under title
    drawHLine(hdc, panelX + 10, panelX + panelW - 10, panelY + 34, CLR_DIVIDER);

    // Labels for each of the three fields
    const wchar_t* labels[3] = {
        L"Lines per column       (default: 20) :",
        L"Characters per line    (default: 40) :",
        L"Columns per page       (default:  2) :"
    };

    // Draw all three rows, highlighting the active one
    for (int i = 0; i < 3; i++) {
        int rowY = panelY + 54 + i * 50;

        // Label
        SetTextColor(hdc, (i == promptStage) ? CLR_TEXT : CLR_LABEL);
        TextOutW(hdc, panelX + 16, rowY, labels[i], (int)wcslen(labels[i]));

        // Input box
        int boxX = panelX + 16, boxY = rowY + 18, boxW = panelW - 32, boxH = 20;
        RECT boxRect = { boxX, boxY, boxX + boxW, boxY + boxH };
        HBRUSH boxBrush = CreateSolidBrush((i == promptStage) ? CLR_PROMPT_BOX : CLR_BACKGROUND);
        FillRect(hdc, &boxRect, boxBrush);
        DeleteObject(boxBrush);
        drawRect(hdc, boxX, boxY, boxX + boxW, boxY + boxH, (i == promptStage) ? CLR_ACCENT : CLR_DIVIDER);

        // Text inside the box — only shown for the active field
        if (i == promptStage && promptLen > 0) {
            SetTextColor(hdc, CLR_TEXT);
            TextOutW(hdc, boxX + 4, boxY + 2, promptBuffer, promptLen);
        }
    }

    // Instruction at the bottom of the panel
    const wchar_t* hint = L"Type a number and press Enter to confirm each field";
    SIZE hintSz;
    GetTextExtentPoint32W(hdc, hint, (int)wcslen(hint), &hintSz);
    SetTextColor(hdc, CLR_LABEL);
    TextOutW(hdc, panelX + (panelW - hintSz.cx) / 2, panelY + panelH - 20, hint, (int)wcslen(hint));
}

// Draws the decorative background behind all columns
void drawPageBackground(HDC hdc, int colsPerPage, int charsPerLine, int linesPerCol) {
    int pageW = colsPerPage * (charsPerLine * fontWidth) + (colsPerPage - 1) * COL_GAP;
    int pageH = linesPerCol * fontHeight;
    RECT paper = { MARGIN_X - 10, MARGIN_Y, MARGIN_X + pageW + 10, MARGIN_Y + pageH + FOOTER_H };
    HBRUSH paperBrush = CreateSolidBrush(CLR_PAPER);
    FillRect(hdc, &paper, paperBrush);
    DeleteObject(paperBrush);
    drawRect(hdc, paper.left, paper.top, paper.right, paper.bottom, CLR_ACCENT);
}

// Draws vertical dividers between columns and dim column labels above each
void drawColumnChrome(HDC hdc, int colsPerPage, int charsPerLine, int linesPerCol) {
    SelectObject(hdc, smallFont);
    SetBkMode(hdc, TRANSPARENT);

    for (int col = 0; col < colsPerPage; col++) {
        int colLeft = MARGIN_X + col * (charsPerLine * fontWidth + COL_GAP);

        // Column label above the column
        wchar_t label[16] = {};
        int li = 0;
        li = appendWide(label, li, L"Col ", 16);
        appendInt(label, li, col + 1, 16);
        SetTextColor(hdc, CLR_LABEL);
        TextOutW(hdc, colLeft, MARGIN_Y - 18, label, (int)wcslen(label));

        // Vertical divider to the left of each column except the first
        if (col > 0)
            drawVLine(hdc, colLeft - COL_GAP / 2, MARGIN_Y - 5, MARGIN_Y + linesPerCol * fontHeight + 5, CLR_DIVIDER);
    }
}

// Draws the "- N -" page footer below the columns
void drawPageFooter(HDC hdc, int pageNumber, int colsPerPage, int charsPerLine, int linesPerCol) {
    int totalW = colsPerPage * charsPerLine * fontWidth + (colsPerPage - 1) * COL_GAP;
    int footerY = MARGIN_Y + linesPerCol * fontHeight + 4;

    // Horizontal rule above the footer text
    drawHLine(hdc, MARGIN_X - 10, MARGIN_X + totalW + 10, footerY, CLR_DIVIDER);

    // Centred page number text
    wchar_t footerText[32] = {};
    int fi = 0;
    fi = appendWide(footerText, fi, L"- ", 32);
    fi = appendInt(footerText, fi, pageNumber + 1, 32);
    appendWide(footerText, fi, L" -", 32);

    SelectObject(hdc, smallFont);
    SIZE sz;
    GetTextExtentPoint32W(hdc, footerText, (int)wcslen(footerText), &sz);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_FOOTER);
    TextOutW(hdc, MARGIN_X + (totalW - sz.cx) / 2, footerY + 4, footerText, (int)wcslen(footerText));
}

// Draws the side panel showing stats and controls
void drawSidePanel(HDC hdc, RECT& clientRect, Tab* tab, int totalPages) {
    int words, charsWith, charsWithout, sentences;
    getStats(tab->buffer, tab->length, words, charsWith, charsWithout, sentences);
    int readTime = words / 200;
    if (readTime == 0 && words > 0) readTime = 1;

    int pageW = tab->colsPerPage * (tab->charsPerLine * fontWidth) + (tab->colsPerPage - 1) * COL_GAP;
    int pageRight = MARGIN_X + pageW + 10;
    int panelX = clientRect.right - 420;
    if (panelX < pageRight + 20)
        panelX = pageRight + 20;
    int panelRight = clientRect.right;
    if (panelRight - panelX < 140) return; // Too small to draw

    int panelY = MARGIN_Y;
    int panelBottom = clientRect.bottom - STATUS_H - 10;
    if (panelBottom <= panelY + 40) return;

    RECT panel = { panelX, panelY, panelRight, panelBottom };
    HBRUSH panelBrush = CreateSolidBrush(CLR_PROMPT_BG);
    FillRect(hdc, &panel, panelBrush);
    DeleteObject(panelBrush);
    drawRect(hdc, panelX, panelY, panelRight, panelBottom, CLR_ACCENT);

    SelectObject(hdc, smallFont);
    SetBkMode(hdc, TRANSPARENT);

    int textX = panelX + 20;
    int textY = panelY + 20;

    const wchar_t* title = L"STATISTICS";
    SetTextColor(hdc, CLR_ACCENT);
    TextOutW(hdc, textX, textY, title, (int)wcslen(title));
    textY += 25;
    drawHLine(hdc, panelX + 10, panelRight - 10, textY - 5, CLR_DIVIDER);
    textY += 10;

    SetTextColor(hdc, CLR_TEXT);
    wchar_t buf[128] = {};
    int bi = 0;
    bi = appendWide(buf, bi, L"Words:            ", 128);
    appendInt(buf, bi, words, 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    bi = 0; buf[0] = L'\0';
    bi = appendWide(buf, bi, L"Chars (w/ space): ", 128);
    appendInt(buf, bi, charsWith, 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    bi = 0; buf[0] = L'\0';
    bi = appendWide(buf, bi, L"Chars (no space): ", 128);
    appendInt(buf, bi, charsWithout, 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    bi = 0; buf[0] = L'\0';
    bi = appendWide(buf, bi, L"Sentences:        ", 128);
    appendInt(buf, bi, sentences, 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    bi = 0; buf[0] = L'\0';
    bi = appendWide(buf, bi, L"Reading Time:     ", 128);
    bi = appendInt(buf, bi, readTime, 128);
    appendWide(buf, bi, L" min", 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    textY += 15;
    drawHLine(hdc, panelX + 10, panelRight - 10, textY - 5, CLR_DIVIDER);
    textY += 10;

    SetTextColor(hdc, CLR_ACCENT);
    title = L"DOCUMENT INFO";
    TextOutW(hdc, textX, textY, title, (int)wcslen(title)); textY += 25;

    SetTextColor(hdc, CLR_TEXT);
    bi = 0; buf[0] = L'\0';
    bi = appendWide(buf, bi, L"Current Page:     ", 128);
    bi = appendInt(buf, bi, tab->page + 1, 128);
    bi = appendWide(buf, bi, L" / ", 128);
    appendInt(buf, bi, totalPages, 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    bi = 0; buf[0] = L'\0';
    bi = appendWide(buf, bi, L"Alignment:        ", 128);
    appendWide(buf, bi, alignName(tab->align), 128);
    TextOutW(hdc, textX, textY, buf, (int)wcslen(buf)); textY += 22;

    textY += 15;
    drawHLine(hdc, panelX + 10, panelRight - 10, textY - 5, CLR_DIVIDER);
    textY += 10;
    SetTextColor(hdc, CLR_ACCENT);
    title = L"SEARCH HISTORY";
    TextOutW(hdc, textX, textY, title, (int)wcslen(title)); textY += 25;
    SetTextColor(hdc, CLR_TEXT);
    for (int i = 0; i < handler.searchCount && textY < panelBottom - 20; i++) {
        wchar_t sh[180] = {};
        int si = 0;
        si = appendWide(sh, si, L"- ", 180);
        for (int k = 0; handler.searches[i].search[k] != '\0' && si < 170; k++)
            sh[si++] = (wchar_t)(unsigned char)handler.searches[i].search[k];
        sh[si] = L'\0';
        si = appendWide(sh, si, L" (", 180);
        si = appendInt(sh, si, handler.searches[i].matches, 180);
        appendWide(sh, si, L")", 180);
        TextOutW(hdc, textX, textY, sh, (int)wcslen(sh));
        textY += 20;
    }

    textY += 15;
    drawHLine(hdc, panelX + 10, panelRight - 10, textY - 5, CLR_DIVIDER);
    textY += 10;

    SetTextColor(hdc, CLR_ACCENT);
    title = L"CONTROLS";
    TextOutW(hdc, textX, textY, title, (int)wcslen(title)); textY += 25;

    SetTextColor(hdc, CLR_LABEL);
    const wchar_t* ctrls[] = {
        L"Ctrl + N       : New Document",
        L"Ctrl + W       : Close Document",
        L"Ctrl + O       : Open Document",
        L"Ctrl + S       : Save Document",
        L"Ctrl + C/X/V   : Copy / Cut / Paste",
        L"> / <          : Next / Prev Match",
        L"Ctrl + Shift+S : Save All",
        L"Ctrl + Tab     : Next Tab",
        L"Ctrl + Shift+T : Prev Tab",
        L"Ctrl + L/R/E/J : Align L/R/C/Justify",
        L"Ctrl + P       : Toggle Right Panel",
        L"F1             : Help Overlay",
        L"PgUp / PgDn    : Change Page"
    };

    for (int i = 0; i < 12; i++) {
        TextOutW(hdc, textX, textY, ctrls[i], (int)wcslen(ctrls[i]));
        textY += 20;
    }
}

// Draws the status bar at the very bottom of the window
void drawStatusBar(HDC hdc, RECT& clientRect, int currentTabIdx, int totalTabs, int pageNumber, int totalPages, int align, bool dirty) {
    // Background strip
    RECT bar = { 0, clientRect.bottom - STATUS_H, clientRect.right, clientRect.bottom };
    HBRUSH barBrush = CreateSolidBrush(CLR_STATUS_BG);
    FillRect(hdc, &bar, barBrush);
    DeleteObject(barBrush);

    // Top border of the status bar
    drawHLine(hdc, 0, clientRect.right, bar.top, CLR_ACCENT);

    // Left segment: tab info, page, alignment
    wchar_t left[128] = {};
    int li = 0;
    li = appendWide(left, li, L"  [Doc ", 128);
    li = appendInt(left, li, currentTabIdx + 1, 128);
    li = appendWide(left, li, L"/", 128);
    li = appendInt(left, li, totalTabs, 128);
    if (dirty)
        li = appendWide(left, li, L"*", 128);
    li = appendWide(left, li, L"]   Page ", 128);
    li = appendInt(left, li, pageNumber + 1, 128);
    li = appendWide(left, li, L" of ", 128);
    li = appendInt(left, li, totalPages, 128);
    li = appendWide(left, li, L"   Align: ", 128);
    appendWide(left, li, alignName(align), 128);
    const wchar_t* right = L"The Aracanum Editor  ";

    SelectObject(hdc, smallFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_STATUS_TEXT);

    int textY = bar.top + (STATUS_H - fontHeight / 2) / 2;
    TextOutW(hdc, 10, textY, left, (int)wcslen(left));

    // Right segment: measure first so it sits flush against the right edge
    SIZE sz;
    GetTextExtentPoint32W(hdc, right, (int)wcslen(right), &sz);
    TextOutW(hdc, clientRect.right - sz.cx, textY, right, (int)wcslen(right));
}

// It is called everytime some event happens on the window
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: { // Runs the first time when a window is created
        // Creating the fonts
        font = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

        // Smaller font for status bar, footer, and column labels
        smallFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

        // Creating the first tab
        handler.tabs[0] = new Tab;
        handler.totalTabs = 1;

        // Getting the height and width of the font
        HDC tempDC = GetDC(hwnd);
        SelectObject(tempDC, font);
        TEXTMETRIC tm;
        GetTextMetrics(tempDC, &tm);
        fontHeight = tm.tmHeight;
        fontWidth = tm.tmAveCharWidth;
        ReleaseDC(hwnd, tempDC);

        // Start the caret blink timer (ID=1, fires every 530ms)
        SetTimer(hwnd, 1, 530, NULL);
        SetTimer(hwnd, 2, 60000, NULL);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == 1 && promptStage == 3) { // Only blink once setup is complete
            caretVisible = !caretVisible;
            Tab* tab = handler.tabs[handler.currentTab];
            Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);

            int firstLine = tab->page * tab->linesPerCol * tab->colsPerPage;
            int lastLine = min(firstLine + (tab->linesPerCol * tab->colsPerPage) - 1, wrap.totalLines - 1);
            int cursorLine = currentLine(*tab, wrap);

            if (cursorLine >= firstLine && cursorLine <= lastLine) {
                int cursorPos = tab->cursor - wrap.array[cursorLine].start;

                int visualSize = wrap.array[cursorLine].size;
                while (visualSize > 0 && (tab->buffer[wrap.array[cursorLine].start + visualSize - 1] == ' ' || tab->buffer[wrap.array[cursorLine].start + visualSize - 1] == '\n')) {
                    visualSize--;
                }
                int xOffset = 0;
                if (tab->align == 2) xOffset = (tab->charsPerLine - visualSize) * fontWidth;
                else if (tab->align == 3) xOffset = ((tab->charsPerLine - visualSize) * fontWidth) / 2;

                int cursorCol = (cursorLine - firstLine) / tab->linesPerCol;
                int cx = MARGIN_X + cursorPos * fontWidth + cursorCol * (tab->charsPerLine * fontWidth + COL_GAP) + xOffset;
                int cy = MARGIN_Y + ((cursorLine - firstLine) % tab->linesPerCol) * fontHeight;
                RECT caretRect = { cx - 1, cy, cx + 3, cy + fontHeight };
                InvalidateRect(hwnd, &caretRect, FALSE);
            }

            deleteLayout(wrap);
        }
        else if (wParam == 2 && promptStage == 3) {
            autoSaveCurrentTab();
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: { // Runs everytime window needs to be drawn
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // Show the setup screen until all three fields are confirmed
        if (promptStage < 3) {
            drawPromptScreen(hdc, clientRect);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // Full background
        HBRUSH bgBrush = CreateSolidBrush(CLR_BACKGROUND);
        FillRect(hdc, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        Tab* tab = handler.tabs[handler.currentTab];

        // Page paper area
        drawPageBackground(hdc, tab->colsPerPage, tab->charsPerLine, tab->linesPerCol);

        // Column dividers and labels
        drawColumnChrome(hdc, tab->colsPerPage, tab->charsPerLine, tab->linesPerCol);

        // Build the layout
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, font);
        SetTextColor(hdc, CLR_TEXT);

        Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);

        // Calculates the number of line which is the first and last line of the concerned page
        int firstLine = tab->page * tab->linesPerCol * tab->colsPerPage;
        int lastLine = min(firstLine + (tab->linesPerCol * tab->colsPerPage) - 1, wrap.totalLines - 1);
        int cursor = tab->cursor;
        int anchor = tab->anchor;
        bool flag = (cursor > anchor) ? true : false;

        // Outputs every line sequentially
        int searchLen = narrowLen(handler.searched);
        for (int i = firstLine; i <= lastLine; i++) {
            int lineStart = wrap.array[i].start;
            int lineEnd = wrap.array[i].end;
            int colIndex = (i - firstLine) / tab->linesPerCol;
            int x = MARGIN_X + colIndex * (tab->charsPerLine * fontWidth + COL_GAP);
            int y = MARGIN_Y + ((i - firstLine) % tab->linesPerCol) * fontHeight;

            int lineSize = wrap.array[i].size;
            int visualSize = lineSize;
            while (visualSize > 0 && (tab->buffer[wrap.array[i].start + visualSize - 1] == ' ' || tab->buffer[wrap.array[i].start + visualSize - 1] == '\n')) {
                visualSize--;
            }

            int xOffset = 0;
            if (tab->align == 2) { // Right
                xOffset = (tab->charsPerLine - visualSize) * fontWidth;
            } else if (tab->align == 3) { // Center
                xOffset = ((tab->charsPerLine - visualSize) * fontWidth) / 2;
            }
            x += xOffset;

            int selectionStart = flag ? anchor : cursor;
            int selectionEnd = flag ? cursor : anchor;

            // Draw the highlighted portion
            if (anchor != -1 && lineStart <= selectionEnd && lineEnd >= selectionStart) {
                int highlightStart = (lineStart > selectionStart) ? lineStart : selectionStart;
                int highlightEnd = (lineEnd < selectionEnd) ? lineEnd : selectionEnd;
                int charsFromStart = highlightStart - lineStart;
                int charsToEnd = highlightEnd - lineStart;
                RECT rectangle = { x + charsFromStart * fontWidth, y, x + charsToEnd * fontWidth, y + fontHeight };
                HBRUSH highlightBrush = CreateSolidBrush(CLR_HIGHLIGHT);
                FillRect(hdc, &rectangle, highlightBrush);
                DeleteObject(highlightBrush);
            }

            if (searchLen > 0) {
                for (int p = lineStart; p + searchLen <= lineEnd; p++) {
                    bool ok = true;
                    for (int s = 0; s < searchLen; s++) {
                        char a = tab->buffer[p + s];
                        char b = handler.searched[s];
                        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                        if (a != b) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) {
                        int c1 = p - lineStart;
                        RECT findRect = { x + c1 * fontWidth, y, x + (c1 + searchLen) * fontWidth, y + fontHeight };
                        HBRUSH findBrush = CreateSolidBrush(CLR_FIND_HIGHLIGHT);
                        FillRect(hdc, &findRect, findBrush);
                        DeleteObject(findBrush);
                        p += searchLen - 1;
                    }
                }
            }

            // Convert  buffer to wide chars for TextOutW
            wchar_t wideLine[512] = {};
            for (int k = 0; k < lineSize && k < 511; k++)
                wideLine[k] = (wchar_t)(unsigned char)*(tab->buffer + wrap.array[i].start + k);

            SetTextColor(hdc, CLR_TEXT);
            if (tab->align == 4 && !wrap.array[i].endedPeacefully && visualSize > 0) {
                int spaces = 0;
                for (int k = 0; k < visualSize; k++) {
                    if (wideLine[k] == L' ')
                        spaces++;
                }

                if (spaces > 0 && visualSize < tab->charsPerLine) {
                    int extraPixels = (tab->charsPerLine - visualSize) * fontWidth;
                    int perSpace = extraPixels / spaces;
                    int rem = extraPixels % spaces;

                    int dx = x;
                    for (int k = 0; k < visualSize && k < 511; k++) {
                        wchar_t ch = wideLine[k];
                        TextOutW(hdc, dx, y, &ch, 1);
                        dx += fontWidth;
                        if (ch == L' ') {
                            dx += perSpace;
                            if (rem > 0) {
                                dx++;
                                rem--;
                            }
                        }
                    }
                }
                else {
                    TextOutW(hdc, x, y, wideLine, lineSize);
                }
            }
            else {
                TextOutW(hdc, x, y, wideLine, lineSize);
            }
        }

        // Draw the blinking cursor as a filled 2px wide rectangle
        if (caretVisible) {
            int cursorLine = currentLine(*tab, wrap);
            if (cursorLine >= firstLine && cursorLine <= lastLine) {
                int cursorPos = cursor - wrap.array[cursorLine].start;

                int visualSize = wrap.array[cursorLine].size;
                while (visualSize > 0 && (tab->buffer[wrap.array[cursorLine].start + visualSize - 1] == ' ' || tab->buffer[wrap.array[cursorLine].start + visualSize - 1] == '\n')) {
                    visualSize--;
                }
                int xOffset = 0;
                if (tab->align == 2) xOffset = (tab->charsPerLine - visualSize) * fontWidth;
                else if (tab->align == 3) xOffset = ((tab->charsPerLine - visualSize) * fontWidth) / 2;

                int cursorCol = (cursorLine - firstLine) / tab->linesPerCol;
                int cx = MARGIN_X + cursorPos * fontWidth + cursorCol * (tab->charsPerLine * fontWidth + COL_GAP) + xOffset;
                int cy = MARGIN_Y + ((cursorLine - firstLine) % tab->linesPerCol) * fontHeight;
                RECT caretRect = { cx, cy, cx + 2, cy + fontHeight };
                HBRUSH caretBrush = CreateSolidBrush(CLR_ACCENT);
                FillRect(hdc, &caretRect, caretBrush);
                DeleteObject(caretBrush);
            }
        }

        // Page footer (- N -) and status bar
        drawPageFooter(hdc, tab->page, tab->colsPerPage, tab->charsPerLine, tab->linesPerCol);
        if (showSidePanel)
            drawSidePanel(hdc, clientRect, tab, wrap.totalPages);
        drawStatusBar(hdc, clientRect, handler.currentTab, handler.totalTabs, tab->page, wrap.totalPages, tab->align, tab->dirty);

        if (showHelpOverlay)
            drawHelpOverlay(hdc, clientRect);

        drawSearchBar(hdc, clientRect);

        deleteLayout(wrap); // Deletes the layout
        EndPaint(hwnd, &ps); // Tells Windows that painting is finished
        return 0;
    }
    case WM_CHAR: { // Runs when a key is pressed which produces a character
        if (promptStage < 3) {
            if (wParam == '\r') { // Enter confirms the current field and moves to the next
                int value = (promptLen > 0) ? _wtoi(promptBuffer) : 0;
                switch (promptStage) {
                case 0: handler.tabs[0]->linesPerCol = (value > 0) ? value : 20; break;
                case 1: handler.tabs[0]->charsPerLine = (value > 0) ? value : 40; break;
                case 2: handler.tabs[0]->colsPerPage = (value > 0) ? value : 2;  break;
                }
                promptStage++;
                promptLen = 0;
                promptBuffer[0] = L'\0';
            }
            else if (wParam == VK_BACK) { // Backspace removes the last typed digit
                if (promptLen > 0) {
                    promptLen--;
                    promptBuffer[promptLen] = L'\0';
                }
            }
            else if (wParam >= '0' && wParam <= '9' && promptLen < 3) { // Only digits, max 3 chars
                promptBuffer[promptLen++] = (wchar_t)wParam;
                promptBuffer[promptLen] = L'\0';
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (handler.isSearching) {
            if (wParam == '\r') {
                copyNarrow(handler.searched, handler.searching, 128);
                int matches = countMatches(handler.tabs[handler.currentTab]->buffer, handler.tabs[handler.currentTab]->length, handler.searched);
                handler.found = (matches > 0);
                storeSearch(handler.searched, matches);
                handler.isSearching = false;
            }
            else if (wParam == VK_BACK) {
                int n = narrowLen(handler.searching);
                if (n > 0)
                    handler.searching[n - 1] = '\0';
            }
            else if (wParam >= 32 && wParam <= 126) {
                int n = narrowLen(handler.searching);
                if (n < 127) {
                    handler.searching[n] = (char)wParam;
                    handler.searching[n + 1] = '\0';
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (wParam == '>') {
            jumpToSearchMatch(true);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        else if (wParam == '<') {
            jumpToSearchMatch(false);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // Ignore character input when Ctrl is held
        if (GetKeyState(VK_CONTROL) & 0x8000)
            return 0;

        // Normal editing
        switch (wParam) {
        case VK_BACK:
            backSpaceCharacter(*handler.tabs[handler.currentTab]);
            break;
        case '\r':
            insertCharacter(*handler.tabs[handler.currentTab], '\n');
            break;
        default:
            insertCharacter(*handler.tabs[handler.currentTab], (char)wParam);
        }
        handler.tabs[handler.currentTab]->dirty = true;

        {
            Wrapper wrap = buildLayout(handler.tabs[handler.currentTab]->buffer, handler.tabs[handler.currentTab]->length, handler.tabs[handler.currentTab]->linesPerCol, handler.tabs[handler.currentTab]->charsPerLine, handler.tabs[handler.currentTab]->colsPerPage);
            int cursorLineIdx = currentLine(*handler.tabs[handler.currentTab], wrap);
            int linesPerPage = handler.tabs[handler.currentTab]->linesPerCol * handler.tabs[handler.currentTab]->colsPerPage;
            handler.tabs[handler.currentTab]->page = cursorLineIdx / linesPerPage;
            deleteLayout(wrap);
        }

        InvalidateRect(hwnd, NULL, FALSE); // Redrawing the window
        return 0;
    }
    case WM_KEYDOWN: { // Runs when a key is pressed which doesn't produce a character
        if (promptStage < 3) return 0; // Arrow keys and navigation do nothing during setup

        if (wParam == VK_F1) {
            showHelpOverlay = !showHelpOverlay;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        bool ctrl = GetKeyState(VK_CONTROL) & 0x8000;
        bool shift = GetKeyState(VK_SHIFT) & 0x8000;

        if (handler.isSearching) {
            if (wParam == VK_ESCAPE) {
                handler.isSearching = false;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        if (ctrl) {
            if (wParam == 'N') {
                if (handler.canOpenTab()) {
                    Tab* newTab = new Tab(
                        handler.tabs[0]->linesPerCol,
                        handler.tabs[0]->charsPerLine,
                        handler.tabs[0]->colsPerPage
                    );
                    if (handler.addTab(newTab)) {
                        handler.currentTab = handler.totalTabs - 1;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    else {
                        delete newTab;
                    }
                }
                return 0;
            } else if (wParam == 'C') {
                copySelectionToClipboard(hwnd, handler.tabs[handler.currentTab]);
                return 0;
            } else if (wParam == 'X') {
                if (cutSelectionToClipboard(hwnd, handler.tabs[handler.currentTab]))
                    InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'V') {
                if (pasteFromClipboard(hwnd, handler.tabs[handler.currentTab]))
                    InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'P') {
                showSidePanel = !showSidePanel;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'F') {
                handler.isSearching = true;
                handler.searching[0] = '\0';
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'S' && shift) {
                for (int i = 0; i < handler.totalTabs; i++) {
                    wchar_t path[64] = {};
                    tabFilePath(i, path, 64);
                    saveTabToPath(handler.tabs[i], path);
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'S') {
                wchar_t path[64] = {};
                tabFilePath(handler.currentTab, path, 64);
                saveTabToPath(handler.tabs[handler.currentTab], path);
                return 0;
            } else if (wParam == 'O') {
                wchar_t path[64] = {};
                tabFilePath(handler.currentTab, path, 64);
                loadTabFromPath(handler.tabs[handler.currentTab], path);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'W') {
                if (handler.totalTabs > 1) {
                    delete handler.tabs[handler.currentTab];
                    for (int i = handler.currentTab; i < handler.totalTabs - 1; i++)
                        handler.tabs[i] = handler.tabs[i + 1];
                    handler.totalTabs--;
                    if (handler.currentTab >= handler.totalTabs)
                        handler.currentTab = handler.totalTabs - 1;
                }
                else {
                    Tab* t = handler.tabs[0];
                    for (int i = 0; i < t->capacity; i++)
                        t->buffer[i] = '\0';
                    t->length = 0;
                    t->cursor = 0;
                    t->anchor = -1;
                    t->page = 0;
                    t->dirty = false;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else if (wParam == 'L') {
                handler.tabs[handler.currentTab]->align = 1; InvalidateRect(hwnd, NULL, FALSE); return 0;
            } else if (wParam == 'R') {
                handler.tabs[handler.currentTab]->align = 2; InvalidateRect(hwnd, NULL, FALSE); return 0;
            } else if (wParam == 'E') { /* Center */
                handler.tabs[handler.currentTab]->align = 3; InvalidateRect(hwnd, NULL, FALSE); return 0;
            } else if (wParam == 'J') {
                handler.tabs[handler.currentTab]->align = 4; InvalidateRect(hwnd, NULL, FALSE); return 0;
            } else if (wParam == VK_TAB) {
                if (shift) {
                    handler.currentTab = (handler.currentTab - 1 + handler.totalTabs) % handler.totalTabs;
                } else {
                    handler.currentTab = (handler.currentTab + 1) % handler.totalTabs;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        switch (wParam) {
        case VK_DELETE:
            deleteCharacter(*handler.tabs[handler.currentTab]);
            handler.tabs[handler.currentTab]->dirty = true;
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
        case VK_NEXT: {
            Wrapper tempWrap = buildLayout(handler.tabs[handler.currentTab]->buffer, handler.tabs[handler.currentTab]->length, handler.tabs[handler.currentTab]->linesPerCol, handler.tabs[handler.currentTab]->charsPerLine, handler.tabs[handler.currentTab]->colsPerPage);
            handler.tabs[handler.currentTab]->page += (handler.tabs[handler.currentTab]->page < tempWrap.totalPages - 1) ? 1 : 0;
            deleteLayout(tempWrap);
            break;
        }
        case VK_PRIOR:
            handler.tabs[handler.currentTab]->page -= (handler.tabs[handler.currentTab]->page > 0) ? 1 : 0;
            break;
        }

        if (wParam == VK_DELETE || wParam == VK_HOME || wParam == VK_END || wParam == VK_UP || wParam == VK_DOWN || wParam == VK_RIGHT || wParam == VK_LEFT) {
            Wrapper wrap = buildLayout(handler.tabs[handler.currentTab]->buffer, handler.tabs[handler.currentTab]->length, handler.tabs[handler.currentTab]->linesPerCol, handler.tabs[handler.currentTab]->charsPerLine, handler.tabs[handler.currentTab]->colsPerPage);
            int cursorLineIdx = currentLine(*handler.tabs[handler.currentTab], wrap);
            int linesPerPage = handler.tabs[handler.currentTab]->linesPerCol * handler.tabs[handler.currentTab]->colsPerPage;
            handler.tabs[handler.currentTab]->page = cursorLineIdx / linesPerPage;
            deleteLayout(wrap);
        }

        InvalidateRect(hwnd, NULL, FALSE); // Redrawing the window
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (promptStage < 3) return 0;
        int mx = LOWORD(lParam);
        int my = HIWORD(lParam);

        Tab* tab = handler.tabs[handler.currentTab];
        Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);

        int firstLine = tab->page * tab->linesPerCol * tab->colsPerPage;

        int col = -1;
        for (int c = 0; c < tab->colsPerPage; c++) {
            int colLeft = MARGIN_X + c * (tab->charsPerLine * fontWidth + COL_GAP);
            int colRight = colLeft + tab->charsPerLine * fontWidth;
            if (mx >= colLeft && mx <= colRight + COL_GAP / 2) {
                col = c;
                break;
            }
        }

        if (col != -1 && my >= MARGIN_Y && my <= MARGIN_Y + tab->linesPerCol * fontHeight) {
            int r = (my - MARGIN_Y) / fontHeight;
            int lineIndex = firstLine + col * tab->linesPerCol + r;

            if (lineIndex < wrap.totalLines) {
                int colLeft = MARGIN_X + col * (tab->charsPerLine * fontWidth + COL_GAP);

                int visualSize = wrap.array[lineIndex].size;
                while (visualSize > 0 && (tab->buffer[wrap.array[lineIndex].start + visualSize - 1] == ' ' || tab->buffer[wrap.array[lineIndex].start + visualSize - 1] == '\n')) {
                    visualSize--;
                }

                int xOffset = 0;
                if (tab->align == 2) xOffset = (tab->charsPerLine - visualSize) * fontWidth;
                else if (tab->align == 3) xOffset = ((tab->charsPerLine - visualSize) * fontWidth) / 2;

                int charOffset = (mx - colLeft - xOffset) / fontWidth;

                if (charOffset < 0) charOffset = 0;
                if (charOffset > wrap.array[lineIndex].size) charOffset = wrap.array[lineIndex].size;

                tab->cursor = wrap.array[lineIndex].start + charOffset;
                if (tab->cursor > tab->length) tab->cursor = tab->length;
            }
        }
        tab->anchor = -1;
        tab->beingSelected = false;

        deleteLayout(wrap);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (promptStage < 3) return 0;
        if (wParam & MK_LBUTTON) {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            Tab* tab = handler.tabs[handler.currentTab];
            Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);

            int firstLine = tab->page * tab->linesPerCol * tab->colsPerPage;

            int col = -1;
            for (int c = 0; c < tab->colsPerPage; c++) {
                int colLeft = MARGIN_X + c * (tab->charsPerLine * fontWidth + COL_GAP);
                int colRight = colLeft + tab->charsPerLine * fontWidth;
                if (mx >= colLeft && mx <= colRight + COL_GAP / 2) {
                    col = c;
                    break;
                }
            }

            if (col != -1 && my >= MARGIN_Y && my <= MARGIN_Y + tab->linesPerCol * fontHeight) {
                int r = (my - MARGIN_Y) / fontHeight;
                int lineIndex = firstLine + col * tab->linesPerCol + r;

                if (lineIndex < wrap.totalLines) {
                    int colLeft = MARGIN_X + col * (tab->charsPerLine * fontWidth + COL_GAP);

                    int visualSize = wrap.array[lineIndex].size;
                    while (visualSize > 0 && (tab->buffer[wrap.array[lineIndex].start + visualSize - 1] == ' ' || tab->buffer[wrap.array[lineIndex].start + visualSize - 1] == '\n')) {
                        visualSize--;
                    }

                    int xOffset = 0;
                    if (tab->align == 2) xOffset = (tab->charsPerLine - visualSize) * fontWidth;
                    else if (tab->align == 3) xOffset = ((tab->charsPerLine - visualSize) * fontWidth) / 2;

                    int charOffset = (mx - colLeft - xOffset) / fontWidth;

                    if (charOffset < 0) charOffset = 0;
                    if (charOffset > wrap.array[lineIndex].size) charOffset = wrap.array[lineIndex].size;

                    if (tab->anchor == -1) tab->anchor = tab->cursor;

                    tab->beingSelected = true;

                    tab->cursor = wrap.array[lineIndex].start + charOffset;
                    if (tab->cursor > tab->length) tab->cursor = tab->length;
                }
            }
            deleteLayout(wrap);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        if (promptStage < 3) return 0;

        HMENU popup = CreatePopupMenu();
        if (!popup) return 0;

        AppendMenuW(popup, MF_STRING, MENU_CUT, L"Cut");
        AppendMenuW(popup, MF_STRING, MENU_COPY, L"Copy");
        AppendMenuW(popup, MF_STRING, MENU_PASTE, L"Paste");
        AppendMenuW(popup, MF_SEPARATOR, 0, NULL);
        AppendMenuW(popup, MF_STRING, MENU_FIND, L"Find");
        AppendMenuW(popup, MF_SEPARATOR, 0, NULL);
        AppendMenuW(popup, MF_STRING, MENU_ALIGN_LEFT, L"Align Left");
        AppendMenuW(popup, MF_STRING, MENU_ALIGN_RIGHT, L"Align Right");
        AppendMenuW(popup, MF_STRING, MENU_ALIGN_CENTER, L"Align Center");
        AppendMenuW(popup, MF_STRING, MENU_ALIGN_JUSTIFY, L"Align Justify");

        POINT p = { LOWORD(lParam), HIWORD(lParam) };
        ClientToScreen(hwnd, &p);

        UINT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, NULL);
        DestroyMenu(popup);

        Tab* tab = handler.tabs[handler.currentTab];
        bool redraw = false;
        if (cmd == MENU_CUT) {
            redraw = cutSelectionToClipboard(hwnd, tab);
        }
        else if (cmd == MENU_COPY) {
            copySelectionToClipboard(hwnd, tab);
        }
        else if (cmd == MENU_PASTE) {
            redraw = pasteFromClipboard(hwnd, tab);
        }
        else if (cmd == MENU_FIND) {
            handler.isSearching = true;
            handler.searching[0] = '\0';
            redraw = true;
        }
        else if (cmd == MENU_ALIGN_LEFT) {
            tab->align = 1;
            redraw = true;
        }
        else if (cmd == MENU_ALIGN_RIGHT) {
            tab->align = 2;
            redraw = true;
        }
        else if (cmd == MENU_ALIGN_CENTER) {
            tab->align = 3;
            redraw = true;
        }
        else if (cmd == MENU_ALIGN_JUSTIFY) {
            tab->align = 4;
            redraw = true;
        }

        if (redraw)
            InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (promptStage < 3) return 0;
        int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        Tab* tab = handler.tabs[handler.currentTab];
        Wrapper wrap = buildLayout(tab->buffer, tab->length, tab->linesPerCol, tab->charsPerLine, tab->colsPerPage);

        if (wheelDelta < 0) { // Scroll down
            if (tab->page < wrap.totalPages - 1) tab->page++;
        } else if (wheelDelta > 0) { // Scroll up
            if (tab->page > 0) tab->page--;
        }
        deleteLayout(wrap);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_DESTROY: {
        KillTimer(hwnd, 1); // Stop the blink timer before destroying
        KillTimer(hwnd, 2);
        PostQuitMessage(0);
        DeleteObject(font);
        DeleteObject(smallFont);
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
    wc.hCursor = NULL;

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
        DispatchMessage(&msg);  // Sends this WM_CHAR to WndProc
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
        if (i - start >= charsPerLine) {
            if (*(buffer + i) == '\n') {
                currentLine.start = start;
                currentLine.end = i;
                currentLine.size = i - start;
                currentLine.endedPeacefully = true;
                start = i + 1;
                j = start;
            }
            else if (j > start) {
                // Normal word wrap
                currentLine.start = start;
                currentLine.end = j;
                currentLine.size = j - start;
                currentLine.endedPeacefully = false;

                start = j + 1; // Start next line after the space
                j = start;

                i = start - 1;
            }
            else {
                continue;
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
        lastActiveLine = { length, length, 0, true };
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
    obj.anchor = -1;
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
    obj.anchor = -1;
    return;
}

// Delete a character (with backspace)
void backSpaceCharacter(Tab& obj) {
    if (obj.beingSelected && obj.anchor != -1 && obj.anchor != obj.cursor)
        deleteRange(obj, ((obj.anchor > obj.cursor) ? obj.cursor : obj.anchor), ((obj.anchor > obj.cursor) ? obj.anchor : obj.cursor));
    else if (obj.cursor != 0) {
        deleteRange(obj, obj.cursor - 1, obj.cursor);
        obj.cursor--;
    }
    return;
}

// Delete a character (with delete)
void deleteCharacter(Tab& obj) {
    if (obj.beingSelected && obj.anchor != -1 && obj.anchor != obj.cursor)
        deleteRange(obj, ((obj.anchor > obj.cursor) ? obj.cursor : obj.anchor), ((obj.anchor > obj.cursor) ? obj.anchor : obj.cursor));
    else if (obj.cursor != obj.length)
        deleteRange(obj, obj.cursor, obj.cursor + 1);
    return;
}

// Movement of cursor, with arrow keys

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
    else {
        if (obj.page == 0) obj.cursor = 0;
        else {
            int prevLine = obj.page * obj.linesPerCol * obj.colsPerPage - 1;
            obj.page--;
            obj.cursor = input.array[prevLine].end;
        }
    }
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
    else {
        if (obj.page >= input.totalPages) obj.cursor = obj.length;
        else {
            int nextLine = (obj.page + 1) * obj.linesPerCol * obj.colsPerPage;
            obj.page++;
            obj.cursor = input.array[nextLine].start;
        }
    }
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