---
name: zx-spectrum-run
description: Run a ZX Spectrum program (.tap, .tzx, .sna, .z80 and similar) in the Fuse emulator and look at what it displays, by screenshotting the emulator. Use this whenever you need to run, test, debug or verify anything on the Spectrum, including a tape or snapshot you just built with z88dk or another toolchain, checking a program's screen output, pressing keys in a running Spectrum program, or comparing behaviour on 48K and 128K machines, even if the user only says "try it" or "does it work" and doesn't mention Fuse or screenshots.
---

# Run a ZX Spectrum program and see its output

Spectrum programs write to emulated video memory, not stdout, so the only way to know what one did is to look at the emulator's screen. `scripts/zx-run.sh` starts Fuse, loads the program, waits until the screen stops changing, saves a screenshot and closes Fuse.

## Workflow

1. Run the script, with `--out` pointing at a temporary location such as your scratchpad:

   ```bash
   <skill-dir>/scripts/zx-run.sh --out /path/to/screen.png program.tap
   ```

   Set the Bash tool's timeout above the script's own `--timeout`. The default of 60 seconds per stage fits inside a 120-second tool timeout only without `--keys` stages.

2. Open the PNG with the Read tool and look at it. Report what is actually on screen, because the exit status only says that the screen stopped changing. A crashed program, a BASIC error report and a correct result all look "settled".

3. Check the bottom line as well as the program output. After a program returns to BASIC, the report there is informative: `0 OK, 40:1` is a clean finish, while something like `R Tape loading error` or `C Nonsense in BASIC` explains a failure. A screen full of garbage or the copyright message usually means a crash or reset.

## Exit status

| Status | Meaning | What to do |
|---|---|---|
| 0 | Screen settled; screenshot saved | Read the screenshot |
| 3 | Screen still changing at the timeout; screenshot saved | Read it anyway. Animation, a flashing cursor or a long computation all do this, but so does a runaway loop, so don't assume it's harmless for a program that should finish |
| 1 | Fuse or a required tool failed | Read the message on stderr |
| 2 | Bad arguments | Run with no arguments to see usage |

## Options

- `--machine 48|128|plus2|plus3|...` picks the model, default 48. Worth trying a second model when compatibility matters; the script's auto-load handles the 128K menu.
- `--min-wait`, `--settle`, `--timeout` control the waiting. A program that computes for a while with nothing changing on screen looks finished, so raise `--settle` above its longest silent stretch. Tape auto-loading has a quiet stretch at boot, which is why `--min-wait` defaults to 8 seconds.
- `--keys "k Return"` presses keys once the screen settles, then waits for it to settle again. Repeat the option for further stages. Names are xdotool key names (`space`, `Return`, `y`, `shift+p`). Only the final screen is saved, so to inspect an intermediate state, run again with fewer stages.
- `--scale 200` enlarges with sharp pixels. Use it when reading numbers, since the Spectrum font's 3, 5, 6, 8 and 9 are easy to confuse at normal size.
- `-- args` passes anything after it to Fuse. For example, `-- --no-traps` loads the tape through the emulated tape signal rather than Fuse's shortcut, which exercises real tape loading and takes longer.

Keys in a Spectrum BASIC prompt are interpreted as keywords, so `k` then `Return` at a BASIC report runs `LIST`.

## Things to know

- A real Fuse window opens on the user's display and takes focus for the length of the run. The capture reads pixels from the screen, so whatever covers the window would end up in the screenshot, and a user clicking over it can spoil a run. If a screenshot shows something other than Fuse, simply rerun.
- Run one at a time, for the same reason.
- `DISPLAY` must be set; the script stops with a clear message otherwise.
- Sound is turned off. Fuse's settings file is not changed.
