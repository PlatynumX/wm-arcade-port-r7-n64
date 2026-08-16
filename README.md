# WWF WrestleMania Arcade portable C port — r7

N64-first, portable-core source translation experiment based on the original Midway WWF WrestleMania source tree.

r7 turns the successful r6h4 hardware experiments into a clean baseline and fixes the missing N64 B-button path before the next combat/rope expansion.

## r7 hardware target

- Nintendo 64 / libdragon is the primary target.
- Stock 4 MB RAM remains the design floor.
- Real Midway Bret CI8 frames and palettes are generated in CI from the historical source assets.
- RDPQ performs sprite blits, TLUT palette uploads, mirroring and the synchronous no-backlog frame path proven by r6h1+.
- The portable core contains no libdragon dependencies.

## What changed from r6h4

- Locks the hardware-proven channel-2 attachment metadata pair at WIMP tail slots 3/4 (raw entry offsets +38/+40) with the positive delta convention.
- Removes the live metadata scanner controls and returns C-buttons to future gameplay use.
- Replaces `joypad_get_buttons_pressed()` for gameplay buttons with an explicit rising-edge latch derived from the current N64 button state. This is specifically to fix the real-hardware report that A worked while B never fired.
- Debug HUD counts raw A and B rising edges (`PAD A:x B:y`) so one hardware run proves whether the controller path is seeing B.
- B still maps to Bret's source-derived normal kick visual sequence; A maps to punch.
- Tightens the first source-accurate combat slice: the normal punch and kick are only hittable during the exact `ANI_ATTACK_ON` frame in `HRTSEQ2.ASM` rather than the broad provisional r6 visual windows.
- Uses the original normal-attack forward box values as the basis for the portable 2-D collision projection while the full arcade 3-D attack transform is translated.
- Keeps two-layer Bret composition, TMEM-safe CI8 strips, synchronous `rdpq_detach_wait()` rendering, CPU opponent, health/stun/knockback, depth sorting and ring bounds.

## Controls

- Analog stick / D-pad: move P1
- Z + movement: run
- A: punch
- B: kick
- L: toggle CPU AI
- R: reset match
- C-Up: replay translated FINISEQ command-stream smoke test
- Start: toggle debug HUD

## Hardware check for the B-button fix

With the debug HUD visible, each fresh press of A or B increments `PAD A:` or `PAD B:`. A B press should also put P1 into `KICK` and play the Bret kick sequence. If `PAD B` increments but no kick appears, the fault is above the N64 input adapter; if it does not increment, the problem is still in the controller/input path.

## Host verification

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
./build-host/wm_headless
```

## N64 build

The included GitHub Actions workflow installs the pinned libdragon SDK, fetches the historical source, regenerates translated animation tables, converts the needed Bret WIMP frames, builds `wm_arcade_r7.z64`, and publishes it on the quota-free `rom-build` branch.

Local N64 build requires a libdragon installation and `N64_INST`:

```sh
make -j2
```

Output: `wm_arcade_r7.z64`
