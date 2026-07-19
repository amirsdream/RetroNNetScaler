# Local ROMs (not committed)

Place your own dumps here. `*.nes` is gitignored.

Extracted from `D:\Game\AllRetroGames\Nintendo NES\roms` for local PC testing:

| Short name | Source dump | Notes |
|---|---|---|
| `smb.nes` | Super Mario Bros. (JU) [!] | Mapper 0 — default playtest |
| `excitebike.nes` | Excitebike (JU) [!] | Mapper 0 |
| `megaman2.nes` | Mega Man 2 (U) | Mapper 1 (MMC1) — scrolling clarity A/B |
| plus originals with full GoodNES names | various | Same folder |

```bat
build\4kscaler.exe --rom roms\smb.nes --scale tinysr
build\4kscaler.exe --rom roms\megaman2.nes --scale nis
```

F1 cycles Nearest → Bilinear → NIS → TinySR → DLSS.
