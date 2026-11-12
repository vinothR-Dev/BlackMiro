# Miru — status (2026-11-05)

Builds on Windows, web and Android; 12 tests pass (`scripts\run-tests-windows.bat`).
Nothing is committed yet (the folder is not a git repository).

## Done this round

- **Email sign-in / sign-up, no password** (`qml/SignInPage.qml`, `src/app/AppState.*`): local profile
  on the device, "Remember me", sign out in the menu. New boards record the owner email.
- **Theme button** (top right, and on the sign-in page): System / Light / Dark, remembered.
  All colours come from `qml/Theme.qml`.
- **Private boards**: lock button next to the title. Saving asks for the password and encrypts the
  `.board` (AES-256 + HMAC); opening asks for it; export asks for it and encrypts PDF (standard
  AES-128 PDF security) and BOARD outputs. Wrong passwords are refused.
- **Right-click menu** (`qml/BoardContextMenu.qml`, press-and-hold on touch): Copy, Copy as PNG/SVG,
  Duplicate, Delete, Rename, Copy/Paste style, Arrange (front/forward/backward/back), Image tools,
  Lock/Unlock, Export selection, Info.
- **Image toolbar** above a selected image (`qml/ImageToolbar.qml`): name, flip H/V, download
  original (unchanged bytes), alt text, crop (interactive), reset crop, original size, lock, more.
- Locked items can't be moved, resized or deleted (padlock badge).

## Tested by hand (web build in the browser pane)

Sign-in validation and remembered profile; insert image, flip, crop on a flipped image, right-click
menu, lock + blocked delete; make private, save with wrong / right password (encrypted file),
password-protected PDF export; dark / light theme.

## Not tested yet

- Open an encrypted `.board` through the password prompt in the UI (covered by unit tests only).
- Android: new screens not yet checked on the emulator. Desktop: right-click menu checked only on web.

## Not planned (need a server or AI)

Start a Flow, Convert to, comments, Copy link, remove background, verified email login.
