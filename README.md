# War Dogs Artillery

A native Windows utility that reads two X/Y coordinates from the Wardogs game map with Windows OCR, calculates the straight-line distance between them, and provides a deterministic L81 mortar range guide.

## Workflow

1. Open the in-game map while `WardogsClient-Win64-Shipping.exe` is running.
2. Press `Ctrl+Alt+1` or click **OCR player position**.
3. Move or mark the target position and leave its coordinate visible on the map.
4. Press `Ctrl+Alt+2` or click **OCR target position**.
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

OCR captures the center portion of the game client, enlarges it, runs the original-color pass first, then uses thresholded fallback passes, and combines the recognized X and Y labels before parsing. This keeps a threshold pass from replacing a more accurate decimal reading from the original image. A capture attempts to bring the game window to the foreground automatically; Windows OCR must have an installed language recognizer. English is normally provided by the Windows language settings.

## RNG guide

The RNG labels are intentionally not OCR'd. The app uses the fixed L81 reference-label table, from lowest to highest:

```text
132, 187, 240, 290, 340, 385, 430,
470, 510, 545, 578, 609, 637, 661, 684
```

The target range is calculated as distance multiplied by the **Meters per coordinate unit** setting. It defaults to `100.0`, which matches the map-coordinate scale used by the supplied screenshots. Set it to `1.0` if the coordinate values are already meters, or adjust it for another map scale.

The printed values are known reference labels. The overlay selects the nearest one and interpolates its vertical offset using the neighboring label spacing. For example, for a `443 m` target it selects `430M`; because `443` is `13/40` of the way from `430` to `470`, the red guide moves `13/40` of one ladder interval below the optic center. Pull the in-game `430M` line down until it meets that guide. This avoids range OCR entirely, so no Tesseract DLLs or `tessdata` files are required by the application.

The horizontal traverse ladder is handled the same way, using the circular reference steps `0, 15, 30, ... 345`. The nearest printed step is selected with 360-degree wraparound, and the red vertical guide is offset in the opposite direction of the target's fractional difference. For example, if the target is `296°`, the overlay selects `300`; place the in-game `300` step over the red vertical guide and the exact target angle will be centered.

## Hotkeys

- `Ctrl+Alt+1`: capture the first coordinate
- `Ctrl+Alt+2`: capture the second coordinate and calculate distance
- `Ctrl+Alt+O`: enable or disable the overlay
- `Ctrl+Alt+T`: toggle overlay click-through
- `Ctrl+Alt+C`: copy the calculated distance

The application uses a normal topmost layered window that tracks the main window of `WardogsClient-Win64-Shipping.exe`. It is enabled by default and stays click-through unless toggled. It is hidden when the target game is not running or its window is unavailable, and can be disabled with `Ctrl+Alt+O`. It does not inject into DirectX or into the game process. Borderless-windowed games are generally the most reliable target for this kind of overlay; exclusive fullscreen and anti-cheat software may still hide or block any external window.

## Build

The project uses CMake and MSVC. The Release configuration statically links the Microsoft C/C++ runtime. The application only needs its executable at runtime; X/Y recognition uses the Windows OCR API and RNG guidance uses the built-in step table.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```
