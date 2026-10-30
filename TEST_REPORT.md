# Test Report — DDLC for GB300 / SF2000

Date: 2026-10-28
Goal: a complete, playable DDLC port on the 320x240 handheld, with no hardware
FPU, tested on the host runner before linking the console cores.

## Test setup

* **Host runner** (`build/native/test_runner_ddlc`): loads the same libretro
  core as the device, presents a 320x240 RGB565 framebuffer, dumps PPM
  screenshots and counts non-zero audio samples. An autopilot reads the Lua
  game state (`ddlc_debug_query`) and advances the game.
* **MIPS soft-float runner** (`/tmp/ddlc_mips/ddlc_mips`): the same engine and
  game compiled with `mipsel-linux-muslsf-g++ -msoft-float -static`, run under
  `qemu-mipsel`. Used for instruction-set, audio and performance checks.
* **Console cores**: compiled with the frog toolchain
  (`mipsel-mti-elf`, `-msoft-float`) and linked with `sf2000_multicore` for
  GB300V2 and SF2000.

## Automated runs

| Run | Frames | Result |
| --- | --- | --- |
| Fresh boot | 500 | language → warning → warning choice → splash, no Lua errors |
| Act 1 autopilot | 20,000 | title → game, poem minigame completed, chapter 1 reached (cl 588), no Lua errors |
| UI/menu scripts | 750–1,200 | pause, settings, save, load, history, help, choice and title menus exercised |
| MIPS soft-float | 1,000 | full boot to game state, 516,299 non-zero audio samples |

The 20,000-frame run was performed twice (before and after the asset/UI
optimizations). The final run completed in 113 s on the host (~5.6 ms/frame)
with no `Error`, `PANIC` or traceback lines.

## Display and text

* 320x240 RGB565, 60 fps libretro timing.
* The original 1280x720 scene is scaled to the 4:3 panel.
* Handheld-specific UI:
  * dialogue box 316x72 px with 16 px Aller font, name box and page-turn
    indicator; line wrapping uses the available 301 px width and selects
    16/14/12 px per paragraph so more words fit on each line;
  * choice panel, pause, settings, save/load, history, help and language
    menus redrawn at 12–16 px;
  * full-screen 320x240 name-entry keyboard with 24 px keys, 12–16 px labels
    and on-screen Space/Delete/Enter/Caps keys;
  * poem minigame word list increased to 36 px logical and columns widened so
    long words do not overlap.
* Images are pre-scaled by 2x and drawn back at 2x scale to cut decode time
  and source-image memory by ~4x (the affected image set is 15.6 MB instead
  of 56.9 MB; game data is ~65 MB).

## Audio

The engine was extended with a streaming `stb_vorbis` source:

* 44.1 kHz OGG sources are resampled to the 22.05 kHz libretro output with a
  16.16 fixed-point linear interpolator and a 2-tap anti-alias average when
  downsampling, which removes most folded high-frequency hash.
* The core emits 367 or 368 frames per video frame so the average is exactly
  22,050 Hz (600 frames = 10.000 s), preventing gradual buffer drift/pops.
* The frontend mono mix now sums L+R in 32 bits before shifting, avoiding
  per-channel LSB truncation.
* Static SFX are replayed from the shared source on handhelds to avoid
  re-decoding and GC churn; `clone()` remains the fallback on other targets.

## No FPU

`objdump -d` on the linked GB300 and SF2000 `core.elf` files:

```
grep -cE '\b(mul|add|sub|cvt|div)\.s|mtc1|mfc1|lwc1|swc1' core.elf
0
```

Both cores are built with `-msoft-float`. The only floating-point code is
soft-float library code (Lua numbers, the 2D scaler).

## Performance

| Target | Time per frame |
| --- | --- |
| Host x86 (32-bit) | ~5.6 ms (~180 fps) |
| qemu-mipsel (soft-float) | ~18 ms (~55 fps) |

The device runs at ~810–918 MHz with a 58–60 MiB core heap. With image scaling
and the reduced asset set, the expected in-game rate is comfortably above
15 fps, which is sufficient for a text-based visual novel. Audio runs
independently at 22.05 kHz.

## What was verified visually

See `screenshots/`:

* Team Salvato splash, content warning and title screen;
* dialogue with Sayori and a full text line;
* content-warning choice panel;
* pause, settings, save, history and help menus;
* poem minigame word list with the 36 px font;
* multi-character classroom scene.

## Known limitations

* The game uses DDLC-LOVE 1.2.3 data, not the original Ren'Py runtime.
* Only English and Spanish are included (the DDLC-LOVE languages).
* The original 16:9 art is stretched to the 4:3 panel.
* Save files are stored in `/ROMS/ddlc/`; the game folder must be writable.
* No shader-based effects; all DDLC effects are image-based.
