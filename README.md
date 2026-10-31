<img align="left" src="logo/raygui_256x256.png" width=256>

**raygui is a simple and easy-to-use immediate-mode-gui library.**

`raygui` was originally inspired by [Unity IMGUI](https://docs.unity3d.com/Manual/GUIScriptingGuide.html) (immediate mode GUI API).

`raygui` was designed as an auxiliary module for [raylib](https://github.com/raysan5/raylib) to create simple GUI interfaces using raylib graphic style (simple colors, plain rectangular shapes, wide borders...) but it can be adapted to other engines/frameworks.

`raygui` is intended for **tools development**; it has already been used to develop [multiple published tools](https://raylibtech.itch.io).

<br>

**WARNING: Latest `raygui` from master branch is always aligned with latest `raylib` from master branch and all raylibtech tools. Make sure to use the latest versions.**

**WARNING: Latest `raygui 5.1` introduces API-BREAKING changes: automatic control capture (manual capture ids removed) and internal text editing (the `editMode` parameter was removed from text controls). A UI frame must be wrapped in `GuiBeginFromRaylib()`/`GuiEndFromRaylib()`.**

*NOTE: raygui is a single-file header-only library (despite its internal dependency on raylib), so, functions definition AND implementation reside in the same file `raygui.h`, when including `raygui.h` in a module, `RAYGUI_IMPLEMENTATION` must be previously defined to include the implementation part of `raygui.h` BUT only in one compilation unit, other modules could also include `raygui.h` but `RAYGUI_IMPLEMENTATION` must not be defined again.*

## features

 - **Immediate-mode gui, no retained data**
 - **+25** controls provided (basic and advanced)
 - Powerful **styling system** for colors, font and metrics
 - Standalone usage mode supported (for other graphic libs)
 - **Icons support**, embedding a complete 1-bit icons pack
 - Multiple **tools** provided for raygui development

## code sample
```c
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

int main()
{
    InitWindow(400, 200, "raygui - controls test suite");
    SetTargetFPS(60);

    bool showMessageBox = false;

    while (!WindowShouldClose())
    {
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            if (GuiButton((Rectangle){ 24, 24, 120, 30 }, "#191#Show Message")) showMessageBox = true;

            if (showMessageBox)
            {
                int btnActive = -1;
                GuiMessageBox((Rectangle){ 85, 70, 250, 100 },
                    "#191#Message Box", "Hi! This is a message!", "Nice;Cool", &btnActive);

                if (btnActive >= 0) showMessageBox = false;
            }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```
![screenshot000](https://github.com/raysan5/raygui/assets/5766837/170e2bce-b7ca-49dc-a263-32b673376546)

## raygui controls

### basic controls
```
Label       |  Button      |  LabelButton |  Toggle      |  ToggleGroup  |  ToggleSlider
CheckBox    |  ComboBox    |  DropdownBox |  TextBox     |  ValueBox     |  Spinner
Slider      |  SliderBar   |  ProgressBar |  StatusBar   |  DummyRec     |  Grid
```
### container/separator controls
```
WindowBox   |  GroupBox    |  Line        |  Panel       |  ScrollPanel  | TabBar
```
### advanced controls
```
ListView    |  ColorPicker |  MessageBox  |  TextInputBox
```


## control capture

Controls are assigned an id automatically in draw order. Every control under the pointer registers in a per-frame hit stack, so overlapping controls (floating panels, popups, scrollable content) resolve a single owner instead of interacting at the same time.

A frame must be started and closed explicitly:

```c
GuiBeginFromRaylib();

if (GuiButton(saveRec, "Save")) save();   // result is returned on release
GuiScrollPanel(panelRec, NULL, contentRec, &scroll, &view);

GuiEndFromRaylib();
```

For a custom backend, inject inputs directly instead of reading raylib:

```c
GuiInput inputs[2] = {
    { { 20, 40 }, true, false, true, 0 },   // position, pressed, released, down, touchId
    { { 240, 40 }, false, false, true, 1 }
};

GuiBegin(inputs, 2);
// ... draw UI ...
GuiEnd();
```

Independent UI regions (for example a left joystick and right buttons) use sibling contexts, each routed to its own touch. Start with `GuiBegin(inputs, count)`, then push/pop in order:

```c
GuiPushContext(&leftContext, leftBounds);
DrawLeftUI();          // only touches inside leftBounds reach this context
GuiPopContext();

GuiPushContext(&rightContext, rightBounds);
DrawRightUI();
GuiPopContext();
```

Contexts are flat (no nesting) and keep their capture across frames. `GuiCancel(&ctx)` silently clears all interaction state (window focus loss, context removal); `GuiReleaseCapture(&ctx)` drops only the capture. `GuiSetDragThreshold(px)` / `GuiGetDragThreshold()` configure the drag threshold at runtime.

Rules:

- The press frame is capture-only: no control runs interaction side effects. The topmost hit control becomes the tentative owner.
- Click-like results are returned on release. Dragging starts after the 5 px drag threshold; direction arbitration then transfers the capture to the first hit control supporting that direction (for example a Slider under a floating Button). If no control supports the direction, the topmost press owner keeps the capture, so a Button still triggers after a small drag.
- The capture owner keeps the interaction while the pointer leaves its bounds, but the release position matters: a discrete control (Button, Toggle, CheckBox, ComboBox, ...) released outside its bounds is cancelled, while a continuous drag (Slider, ScrollBar, ColorPanel) may finish outside and keeps updating.
- The default context uses only the first input of the frame; extra touches are ignored. With `GuiPushContext()`, each context receives one unclaimed touch inside its bounds, and keeps that touch routed until release even if it moves outside the bounds.
- A touch already routed/captured by one context is never delivered to a sibling context. Contexts are isolation units, not concurrency units: use one context per independent touch area.
- `GuiBeginFrame()`/`GuiEndFrame()` are the macro-driven convenience pair used by standalone backends; `GuiBegin()`/`GuiEnd()` take explicit `GuiInput` snapshots. All three frame pairs are paired and cannot be mixed within a frame.
- Without capture only the topmost hit control is highlighted; with capture only the capture holder is. Hover ownership does not drift with the pointer.
- Editing TextBox and expanded DropdownBox are modal: clicking outside only ends their interaction and never activates controls underneath.
- Only one discrete control returns a result per context per frame.
- `GuiLock()` blocks every input path (pointer, keyboard, wheel), including an active text editor, an open dropdown and an in-progress slider drag. Interaction state freezes and resumes after `GuiUnlock()`.
- `GuiGetCapturedId()` returns the active control id (`0` when idle). Ids are internal and are not exposed to the application.
- Control ids are assigned automatically in draw order. Draw dynamic lists whose item count can change **after** all fixed controls: otherwise the ids of the controls drawn after the list shift and an in-progress Slider/ScrollPanel drag can lose its capture.
- TextBox, ValueBox, ValueBoxFloat, Spinner and DropdownBox manage their edit/open state internally. Within a context only one text control can be edited at a time: clicking another one commits the previous editor and switches focus, and keyboard input only reaches the active control. Use `GuiIsEditing()` to check whether the current context has a text control being edited. Keyboard input is global, so do not edit text in two contexts simultaneously.
- ValueBox and ValueBoxFloat are built on the internal GuiTextBox: both keep their text in an internal buffer while editing and parse only on commit (ENTER, ESCAPE or click outside). `GuiValueBoxFloat(bounds, text, float *value)` no longer takes a caller text buffer. Invalid text keeps the previous value; integer values are clamped on commit. This also gives them normal TextBox editing (cursor, selection, clipboard).
- `GuiSpinner()` is a layout helper built from two Buttons and a ValueBox; the three children own their capture ids independently. While the value box is editing, an arrow click first commits the editor (modal), and the next click steps the value.

Migrating from raygui 5.0: remove `GuiSetCapturedId()`, `GuiPushId()` and `GuiPopId()` calls, wrap the UI in `GuiBeginFromRaylib()`/`GuiEndFromRaylib()` (or `GuiBeginFrame()`/`GuiEndFrame()` for a custom backend, or `GuiBegin(inputs, count)`/`GuiEnd()` for explicit inputs), and remove the `editMode` argument from TextBox/ValueBox/ValueBoxFloat/Spinner/DropdownBox calls.

See [examples/scroll_panel](examples/scroll_panel) for overlapping controls and [examples/controls_test_suite](examples/controls_test_suite) for every control.
For the implementation notes and behaviour matrix, see [docs/control-capture.md](docs/control-capture.md).


## raygui styles

`raygui` comes with a [default](styles/default) style automatically loaded at runtime:

![raygui default style](styles/default/style_default.png)

Some additional styles are also provided for convenience, just check [styles directory](styles) for details:

![raygui additional styles](images/raygui_style_table_multi.png)

Custom styles can also be created very easily using [rGuiStyler](https://raylibtech.itch.io/rguistyler) tool.

Styles can be loaded at runtime using raygui `GuiLoadStyle()` function. Simple and easy-to-use.

## raygui icons

`raygui` supports custom icons, by default, a predefined set of icons is provided inside `raygui` as an array of binary data; it contains **512 possible icons** defined as **16x16 pixels** each; each pixel is codified using **1-bit**. The total size of the array is `4096 bytes`.

![raygui icons v5](images/raygui_icons_v5.png)

To use any of those icons just prefix the *#iconId#* number to **any text** written within `raygui` controls:
```c
if (GuiButton(rec, "#05#Open Image")) { /* ACTION */ }
```
It's also possible to use the provided `GuiIconText()` function to prefix it automatically, using a clearer identifier (defined in `raygui.h`).
```c
if (GuiButton(rec, GuiIconText(ICON_FILE_OPEN, "Open Image"))) { /* ACTION */ }
```
Provided set of icons can be reviewed and customized using [rGuiIcons](https://raylibtech.itch.io/rguiicons) tool.

## raygui support tools

 - [**rGuiStyler**](https://raylibtech.itch.io/rguistyler) - A simple and easy-to-use raygui styles editor.

   ![rGuiStyler v6.0](images/rguistyler_v600.png)

 - [**rGuiIcons**](https://raylibtech.itch.io/rguiicons) - A simple and easy-to-use raygui icons editor.

   ![rGuiIcons v4.0](images/rguiicons_v400.png)

 - [**rGuiLayout**](https://raylibtech.itch.io/rguilayout) - A simple and easy-to-use raygui layouts editor.

   ![rGuiLayout v5.0](images/rguilayout_v500.png)

## building

`raygui` is intended to be used as a portable single-file header-only library, to be directly integrated into any C/C++ codebase but some users could require a shared/dynamic version of the library, for example, to create bindings:

 - **Windows (MinGW, GCC)**
```
copy src/raygui.h src/raygui.c
gcc -o src/raygui.dll src/raygui.c -shared -DRAYGUI_IMPLEMENTATION -DBUILD_LIBTYPE_SHARED -static-libgcc -lopengl32 -lgdi32 -lwinmm -Wl,--out-implib,src/librayguidll.a
```

 - **Windows (MSVC)**
```
copy src\raygui.h src\raygui.c
# For `cl` to be accessible open "Developer Command Prompt for VS 2022".
cl /O2 /I<RELATIVE_PATH_TO_RAYLIB_INCLUDE_DIR> /D_USRDLL /D_WINDLL /DRAYGUI_IMPLEMENTATION /DBUILD_LIBTYPE_SHARED src/raygui.c /LD /Feraygui.dll /link /LIBPATH <RELATIVE_PATH_TO_RAYLIB_IMPORT_LIBRARY> msvcrt.lib winmm.lib gdi32.lib user32.lib shell32.lib /subsystem:windows /machine:x64

# Example:
# cl /O2 /I../raylib-6_0_win64_msvc16/include /D_USRDLL /D_WINDLL /DRAYGUI_IMPLEMENTATION /DBUILD_LIBTYPE_SHARED src/raygui.c /LD /Feraygui.dll /link /LIBPATH ../raylib-6_0_win64_msvc16/lib/raylibdll.lib msvcrt.lib winmm.lib gdi32.lib user32.lib shell32.lib /subsystem:windows /machine:x64
```

 - **Linux (GCC)**
```
mv src/raygui.h src/raygui.c
gcc -o raygui.so src/raygui.c -shared -fpic -DRAYGUI_IMPLEMENTATION -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
mv src/raygui.c src/raygui.h
```

- **Mac (clang, homebrew installed raylib)**
```
cp src/raygui.h src/raygui.c
brew install raylib
gcc -o raygui.dynlib src/raygui.c -shared -fpic -DRAYGUI_IMPLEMENTATION -framework OpenGL -lm -lpthread -ldl $(pkg-config --libs --cflags raylib)
```


## license

raygui is licensed under an unmodified zlib/libpng license, which is an OSI-certified, BSD-like license that allows static linking with closed source software. Check [LICENSE](LICENSE) for further details.
