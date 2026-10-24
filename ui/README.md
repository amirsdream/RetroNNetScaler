# RetroNNetScaler UI

Electron front-end (plain HTML/CSS/JS, no bundler, one dependency) for the headless NES core.

```
Electron UI  ── stdin: pad, pause, resume, reset, speed, save, load, quit ──▶  4kscaler-core
             ◀── stdout: raw 256x240 RGBA frames;  stderr: "@ok|@err" replies  (C++, audio via miniaudio)
```

## Run

```sh
cmake --build ../build --target 4kscaler-core   # once, from a configured build dir
npm install                                      # once; only dependency is electron
npm start
```

The core is looked up at `../build/4kscaler-core[.exe]`; override with `RETRO_CORE=/path/to/core`.
ROMs are scanned from `../roms` plus any folder added in the UI.

If Electron opens as plain Node (`Cannot find module 'electron'`), the shell has
`ELECTRON_RUN_AS_NODE` set (VS Code tasks do this) — unset it.

## Features

- Menu bar (File, Emulation, Video, Input, View, Help) with keyboard navigation; Alt opens it
- Home with "continue playing" hero, library grid/list with search, sort and card size, favorites, recent
- Box art from [libretro-thumbnails](https://github.com/libretro-thumbnails) (cached; can be turned off), custom covers
- Save states: 8 slots with thumbnails, plus an automatic resume point when you quit
- Fast-forward, screenshots, play-time tracking, key rebinding, gamepads

## Hotkeys

| Key | Action |
|---|---|
| Esc | Pause menu |
| F5 / F8 | Save / load state (current slot) |
| F6 / F7 | Previous / next slot |
| Tab (hold) | Fast-forward |
| F11 / F12 | Fullscreen / screenshot |
| Ctrl R / Ctrl O | Reset / open ROM |
| Ctrl 1–4, Ctrl B, Ctrl F | Pages, sidebar, search |

Settings, library data, saves and box art live in Electron's `userData` folder (Help → Open Data Folder).
Screenshots go to `Pictures/RetroNNetScaler`.
