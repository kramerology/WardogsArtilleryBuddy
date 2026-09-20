# Arty Buddy

A native Windows utility that reads two X/Y coordinates from the Wardogs game map through the game's coordinate clipboard, calculates the straight-line distance between them, and provides a deterministic L81 mortar range guide.

## Workflow

1. Open the in-game map while `WardogsClient-Win64-Shipping.exe` is running.
2. Put the mouse over the player position and press the configured player-capture hotkey, click **Capture player**, or enter/paste the player coordinates manually.
3. Put the mouse over the target position and press the configured target-capture hotkey, click **Capture target**, or enter/paste the target coordinates manually.
5. The calculated distance appears in the utility and in the topmost overlay.
6. The direction appears as a bearing plus an 8-point compass label, such as `145° SE`.
7. Keep the scope in RNG mode. The utility chooses the nearest known RNG line and moves a red horizontal guide bar to the exact offset needed for the calculated range.
8. Align the nearest horizontal compass step with the red vertical angle guide. Its fractional offset accounts for the difference between the printed 15° step and the exact target bearing.

The parser accepts forms such as:

- `x123.45 y567.89`
- `X: 123.4, Y: 567.8`
- `X=123 Y=567`
- `(123.4, 567.8)`
- `123.4, 567.8`

Direction uses the game's coordinate orientation: positive X is east/right, positive Y is north/up, `0°` is north, and angles increase clockwise.

Capture hotkeys run a short input macro against the foreground game: right-click, move the mouse 10 pixels down and right, left-click, briefly wait for the coordinate field, press `Ctrl+A`, press `Ctrl+C`, press Backspace, and finish with another left-click. A short pause is inserted between each completed macro step, and the cursor is restored to its starting position afterward. The copied coordinate text is read from the Windows clipboard and validated by Arty Buddy; if the first attempt does not produce a valid coordinate pair, it retries once with longer waits. The app briefly hides its layered guides while the macro runs so they cannot receive the clicks. The fixed 10-pixel move is independent of display resolution.

## Manual and clipboard input

The main window includes separate X and Y fields for the player and target. Enter numeric values and click **Apply** for either point. The **Paste** buttons read text from the Windows clipboard and accept coordinate formats including `X = 105.59` / `Y = 110.89`, `X: 105.59, Y: 110.89`, and `105.59, 110.89`. Clipboard input is text-based; copying an image does not provide coordinates.

## RNG guide

The RNG labels are not read from the screen. The app uses the fixed L81 reference-label table, from lowest to highest:

```text
132, 187, 240, 290, 340, 385, 430,
470, 510, 545, 578, 609, 637, 661, 684
```

The target range is calculated as distance multiplied by the fixed map scale of `100.0` meters per coordinate unit, which matches the map-coordinate scale used by the supplied screenshots.

The printed values are known reference labels. The overlay selects the nearest one and interpolates its vertical offset using the neighboring label spacing. For example, for a `443 m` target it selects `430M`; because `443` is `13/40` of the way from `430` to `470`, the red guide moves `13/40` of one ladder interval below the optic center. Pull the in-game `430M` line down until it meets that guide.

The horizontal traverse ladder is handled the same way, using the circular reference steps `0, 15, 30, ... 345`. The nearest printed step is selected with 360-degree wraparound, and the red vertical guide is offset in the opposite direction of the target's fractional difference. For example, if the target is `296°`, the overlay selects `300`; place the in-game `300` step over the red vertical guide and the exact target angle will be centered.

## Hotkeys

The default shortcuts are `Ctrl+Alt+1`, `Ctrl+Alt+2`, `Ctrl+Alt+O`, `Ctrl+Alt+T`, and `Ctrl+Alt+C`. Open the settings page with the cog button to assign any shortcut, including Ctrl, Alt, Shift, and Win modifiers. Click a shortcut button, then press the desired key combination.

The settings page also includes automatic updates, enabled by default. When enabled, Arty Buddy checks the latest stable GitHub release at startup and asks whether to download and install a newer Windows executable. The updater downloads over HTTPS, uses the bundled `ArtyBuddyUpdater.exe` helper to replace the executable after Arty Buddy exits, and restarts the app. The **Exit on close** setting is off by default; when enabled, closing the main Arty Buddy window exits the process instead of leaving it in the tray.

Right-click the tray icon for the context menu. It includes an explicit **Exit** command; double-clicking the icon reopens the main window.

The application uses a normal topmost layered window that tracks the main window of `WardogsClient-Win64-Shipping.exe`. It is enabled by default and stays click-through unless toggled. It is shown only while Wardogs is the foreground, non-minimized window; it hides when the game is not running, minimized, or another window is active. It can also be disabled with `Ctrl+Alt+O`. It does not inject into DirectX or into the game process. Borderless-windowed games are generally the most reliable target for this kind of overlay; exclusive fullscreen and anti-cheat software may still hide or block any external window.

## Build

The project uses CMake and MSVC. The Release configuration statically links the Microsoft C/C++ runtime. The application needs `WarDogsArtillery.exe` and the bundled `ArtyBuddyUpdater.exe` helper at runtime; X/Y capture uses the Windows input and clipboard APIs, and RNG guidance uses the built-in step table.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```
