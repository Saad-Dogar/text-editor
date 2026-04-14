# Copilot Instructions

## Project Guidelines
- User coding constraint: Do not use WinAPI functions outside their allowed list, which includes only the explicit WinAPI allowlist (e.g., RegisterClassW, CreateWindowExW, painting, input, file APIs listed by user). Avoid using other WinAPI functions (e.g., wsprintfW, LoadCursor, GetOpenFileNameW, GetSaveFileNameW).
- Do not add or use any additional libraries/includes without explicit user confirmation first.

## Code Style
- Avoid comments that look AI-generated or instructional; keep only the user's own comments/style when editing code.