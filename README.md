# Text Editor

A lightweight, multi-tab text editor built from scratch in C++ using the raw Win32 API — no rich-edit controls, no third-party UI libraries.

---

## Features

### Multi-Tab Editing
- Open up to 10 tabs simultaneously
- Each tab maintains its own independent buffer, cursor position, alignment, and page state
- Dirty flag (`*`) shown in the status bar when a tab has unsaved changes

### File I/O
- Open files from disk into a tab
- Save the current tab (`Ctrl+S`)
- Save all open tabs at once (`Ctrl+Shift+S`)

### Text Editing
- Full character insertion and deletion (Backspace / Delete)
- Dynamic buffer that grows automatically as you type
- Keyboard-based text selection (Shift + Arrow keys)
- Mouse click-to-position cursor
- Mouse drag selection
- Copy, Cut, and Paste with Windows clipboard integration (`Ctrl+C`, `Ctrl+X`, `Ctrl+V`)

### Find / Search
- Incremental search bar (`Ctrl+F`)
- Case-insensitive matching
- Forward and backward navigation through matches
- Cursor auto-jumps to the matched position and syncs to the correct page

### Text Alignment
- Left, Right, Center, and Justify modes
- Toggle via keyboard shortcuts (`Ctrl+L`, `Ctrl+R`, `Ctrl+E`, `Ctrl+J`) or the right-click context menu

### Page & Column Layout
- Configurable layout at startup: lines per column, characters per line, columns per page
- Multi-column page view — text flows across columns, then paginates
- Page footer displays the current page number
- Mouse wheel and Page Up/Down for page navigation

### UI Chrome
- Status bar showing: tab index, current page, total pages, alignment mode, and dirty state
- Toggleable right-side panel (`Ctrl+P`)
- Help overlay listing all keyboard shortcuts (`F1` or `?`)
- Right-click context menu for common editing operations

---

## Controls

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New tab |
| `Ctrl+W` | Close current tab |
| `Ctrl+S` | Save current tab |
| `Ctrl+Shift+S` | Save all tabs |
| `Ctrl+O` | Open file |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `Ctrl+F` | Find |
| `Ctrl+L/R/E/J` | Align Left / Right / Center / Justify |
| `Ctrl+C/X/V` | Copy / Cut / Paste |
| `Ctrl+P` | Toggle right panel |
| `Arrow Keys` | Move cursor |
| `Shift+Arrow` | Extend selection |
| `Page Up/Down` | Navigate pages |
| `Home/End` | Jump to line start/end |

---

## Architecture

```
text_editor.cpp   →   Win32 WndProc event loop, rendering, UI layout
structure.h       →   Core data structures (Editor, Tab, Layout, Wrapper)
```

The `Wrapper` / `Layout` system is the heart of the editor — it takes the raw flat character buffer and formats it into lines, columns, and pages on every render pass, with no intermediate storage of formatted text.

---

## Building

**Requirements:** Windows, any C++ compiler with Win32 support (MSVC recommended)

```bash
# MSVC
cl text_editor.cpp /link user32.lib gdi32.lib

# MinGW
g++ text_editor.cpp -o TextEditor.exe -mwindows -lgdi32 -luser32
```

---

## Tech Stack

- **Language:** C++
- **Platform:** Windows (Win32 API)
- **Rendering:** Raw GDI — all text, selections, cursors, and UI drawn manually
- **No external libraries**
