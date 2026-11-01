/*******************************************************************************************
*
*   raygui - Control capture core headless tests (phase 1 + phase 2)
*
*   Uses RAYGUI_STANDALONE with a deterministic fake input backend to verify:
*       - automatic ids and topmost hit capture
*       - press frame is capture-only, click results are delivered on release
*       - drag threshold and direction arbitration
*       - hover ownership (topmost hit, capture holder)
*       - modal interception for text boxes and dropdowns
*       - one discrete control trigger per frame
*       - explicit GuiInput injection, sibling contexts and touch routing
*       - GuiCancel / GuiReleaseCapture / runtime drag threshold
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include <stdio.h>
#include <string.h>

#define RAYGUI_STANDALONE

// Fake input is provided through the standalone backend functions; these macros
// route raygui pointer inputs to them (expanded inside the implementation)
#define GUI_BUTTON_DOWN         IsMouseButtonDown(0)
#define GUI_BUTTON_PRESSED      IsMouseButtonPressed(0)
#define GUI_BUTTON_RELEASED     IsMouseButtonReleased(0)
#define GUI_SCROLL_DELTA        GetMouseWheelMove()
#define GUI_POINTER_POSITION    GetMousePosition()
#define GUI_KEY_DOWN(key)       IsKeyDown(key)
#define GUI_KEY_PRESSED(key)    IsKeyPressed(key)
#define GUI_INPUT_KEY           GetCharPressed()

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

//----------------------------------------------------------------------------------
// Fake input state, driven by the test frame helper
//----------------------------------------------------------------------------------
static Vector2 gMouse = { 0, 0 };
static bool gMouseDown = false;
static bool gMousePressed = false;
static bool gMouseReleased = false;
static float gWheel = 0.0f;
static int gKeyPressed = 0;
static int gCharPressed = 0;

//----------------------------------------------------------------------------------
// Fake raylib backend (standalone mode)
//----------------------------------------------------------------------------------
static Rectangle gFontRecs[256] = { 0 };
static GlyphInfo gFontGlyphs[256] = { 0 };

static Vector2 GetMousePosition(void) { return gMouse; }
static float GetMouseWheelMove(void) { return gWheel; }
static bool IsMouseButtonDown(int button) { (void)button; return gMouseDown; }
static bool IsMouseButtonPressed(int button) { (void)button; return gMousePressed; }
static bool IsMouseButtonReleased(int button) { (void)button; return gMouseReleased; }
static bool IsKeyDown(int key) { (void)key; return false; }
static bool IsKeyPressed(int key) { return (key == gKeyPressed)? true : false; }
static int GetCharPressed(void) { return gCharPressed; }
static const char *GetClipboardText(void) { return NULL; }

static void DrawRectangle(int x, int y, int width, int height, Color color) { (void)x; (void)y; (void)width; (void)height; (void)color; }
static void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4) { (void)rec; (void)col1; (void)col2; (void)col3; (void)col4; }

static Font GetFontDefault(void)
{
    Font font = { 0 };
    font.texture.id = 1;
    font.baseSize = 10;
    font.glyphCount = 256;
    font.glyphPadding = 0;
    font.recs = gFontRecs;
    font.glyphs = gFontGlyphs;
    return font;
}

static Font LoadFontEx(const char *fileName, int fontSize, int *codepoints, int codepointCount)
{
    (void)fileName; (void)fontSize; (void)codepoints; (void)codepointCount;
    return GetFontDefault();
}

static Texture2D LoadTextureFromImage(Image image) { (void)image; Texture2D tex = { 0 }; return tex; }
static void UnloadTexture(Texture2D texture) { (void)texture; }
static void SetShapesTexture(Texture2D tex, Rectangle rec) { (void)tex; (void)rec; }

static char *LoadFileText(const char *fileName) { (void)fileName; return NULL; }
static void UnloadFileText(char *text) { (void)text; }
static const char *GetDirectoryPath(const char *filePath) { (void)filePath; return NULL; }
static int *LoadCodepoints(const char *text, int *count) { (void)text; if (count != NULL) *count = 0; return NULL; }
static void UnloadCodepoints(int *codepoints) { (void)codepoints; }
static unsigned char *DecompressData(const unsigned char *compData, int compDataSize, int *dataSize)
{
    (void)compData; (void)compDataSize;
    if (dataSize != NULL) *dataSize = 0;
    return NULL;
}

static Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing)
{
    (void)font; (void)text; (void)fontSize; (void)spacing;
    Vector2 size = { 0, 0 };
    return size;
}

static int GetGlyphIndex(Font font, int codepoint) { (void)font; (void)codepoint; return 0; }
static int GetCodepoint(const char *text, int *codepointSize)
{
    if (codepointSize != NULL) *codepointSize = (text == NULL || text[0] == '\0')? 0 : 1;
    return (text == NULL || text[0] == '\0')? 0 : (int)(unsigned char)text[0];
}
static int GetCodepointPrevious(const char *text, int *codepointSize)
{
    (void)text;
    if (codepointSize != NULL) *codepointSize = 0;
    return 0;
}
static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint)
{
    (void)texture; (void)source; (void)dest; (void)origin; (void)rotation; (void)tint;
}
static void DrawTextCodepoint(Font font, int codepoint, Vector2 position, float fontSize, Color tint)
{
    (void)font; (void)codepoint; (void)position; (void)fontSize; (void)tint;
}
static int GetScreenWidth(void) { return 800; }
static int GetScreenHeight(void) { return 600; }

//----------------------------------------------------------------------------------
// Test helpers
//----------------------------------------------------------------------------------
static int gChecks = 0;
static int gFailures = 0;

#define CHECK(cond, msg) do { \
    gChecks++; \
    if (!(cond)) { gFailures++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

static void ResetGui(void)
{
    memset(&guiCapture, 0, sizeof(guiCapture));
    gWheel = 0.0f;
    gKeyPressed = 0;
    gCharPressed = 0;
    gMouse = RAYGUI_CLITERAL(Vector2){ 0, 0 };
    gMouseDown = false;
    gMousePressed = false;
    gMouseReleased = false;
}

// Start a frame with the given pointer state (the GUI snapshots it at GuiBeginFrame)
static void Begin(Vector2 position, bool down, bool pressed, bool released)
{
    gMouse = position;
    gMouseDown = down;
    gMousePressed = pressed;
    gMouseReleased = released;
    GuiBeginFrame();
}

static void End(void) { GuiEndFrame(); }

// Press on the given position and release on the next frame at the same place
static void PressRelease(Vector2 position, void (*draw)(void))
{
    Begin(position, true, true, false);
    draw();
    End();

    Begin(position, false, false, true);
    draw();
    End();
}

//----------------------------------------------------------------------------------
// Test controls
//----------------------------------------------------------------------------------
static int gLowerClicks = 0;
static int gUpperClicks = 0;
static void DrawOverlappingButtons(void)
{
    Rectangle lower = { 0, 0, 100, 100 };
    Rectangle upper = { 50, 0, 100, 100 };
    if (GuiButton(lower, NULL)) gLowerClicks++;
    if (GuiButton(upper, NULL)) gUpperClicks++;
}

static int gButtonClicks = 0;
static void DrawSingleButton(void)
{
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 }, NULL)) gButtonClicks++;
}

static float gSliderValue = 0.5f;
static int gSliderResult = 0;
static void DrawSliderOnly(void)
{
    gSliderResult = GuiSlider(RAYGUI_CLITERAL(Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &gSliderValue, 0.0f, 1.0f);
}

static int gOverlayButtonClicks = 0;
static void DrawSliderWithButton(void)
{
    gSliderResult = GuiSlider(RAYGUI_CLITERAL(Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &gSliderValue, 0.0f, 1.0f);
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 80, 0, 60, 30 }, NULL)) gOverlayButtonClicks++;
}

static char gTextBoxText[32] = "abc";
static int gTextBoxResult = 0;
static void DrawTextBoxOnly(void)
{
    gTextBoxResult = GuiTextBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, gTextBoxText, 32);
}

static int gUnderlayClicks = 0;
static void DrawTextBoxOverButton(void)
{
    // Button drawn first (bottom), text box on top
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL)) gUnderlayClicks++;
    gTextBoxResult = GuiTextBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, gTextBoxText, 32);
}

static int gValue = 10;
static int gValueResult = 0;
static void DrawValueBoxOnly(void)
{
    gValueResult = GuiValueBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL, &gValue, 0, 100);
}

static int gDropdownActive = 0;
static int gDropdownResult = 0;

static int gUnderDropdownClicks = 0;
static void DrawDropdownOverButton(void)
{
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 0, 0, 200, 200 }, NULL)) gUnderDropdownClicks++;
    gDropdownResult = GuiDropdownBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, "A;B;C", &gDropdownActive);
}

static int gComboActive = 0;
static int gComboResult = 0;
static void DrawComboBox(void)
{
    gComboResult = GuiComboBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, "A;B;C", &gComboActive);
}

static Vector2 gScroll = { 0, 0 };
static int gScrollResult = 0;
static void DrawScrollPanel(void)
{
    Rectangle view = { 0 };
    gScrollResult = GuiScrollPanel(RAYGUI_CLITERAL(Rectangle){ 0, 0, 200, 200 }, NULL,
        RAYGUI_CLITERAL(Rectangle){ 0, 0, 600, 600 }, &gScroll, &view);
}

static int gGridResult = 0;
static Vector2 gGridCell = { -1, -1 };
static void DrawGrid(void)
{
    gGridResult = GuiGrid(RAYGUI_CLITERAL(Rectangle){ 0, 0, 200, 200 }, NULL, 20.0f, 2, &gGridCell);
}

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------
static void TestTopmostWins(void)
{
    ResetGui();
    gLowerClicks = 0;
    gUpperClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, true, true, false);
    DrawOverlappingButtons();
    End();

    CHECK(GuiGetCapturedId() == 2, "topmost control is the tentative owner on press");
    CHECK((gLowerClicks == 0) && (gUpperClicks == 0), "no control triggers on the press frame");

    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, false, false, true);
    DrawOverlappingButtons();
    End();

    CHECK(gUpperClicks == 1, "topmost control triggers on release");
    CHECK(gLowerClicks == 0, "covered control does not trigger");
    CHECK(GuiGetCapturedId() == 2, "captured id is visible during the release frame");

    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, false, false, false);
    DrawOverlappingButtons();
    End();

    CHECK(GuiGetCapturedId() == 0, "capture clears on the next idle frame");
}

static void TestEmptyPressDoesNotClick(void)
{
    ResetGui();
    gButtonClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 20, 20 }, true, true, false);
    DrawSingleButton();
    End();
    CHECK(GuiGetCapturedId() == 1, "button is tentative when pressed over it");

    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, true, true, false);
    DrawSingleButton();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 50 }, false, false, true);
    DrawSingleButton();
    End();

    CHECK(gButtonClicks == 0, "press on empty space cannot be recovered by releasing over a button");
}

static void TestThresholdClickAndDrag(void)
{
    ResetGui();
    gButtonClicks = 0;

    // Click within the threshold
    PressRelease(RAYGUI_CLITERAL(Vector2){ 50, 50 }, DrawSingleButton);
    CHECK(gButtonClicks == 1, "click within the drag threshold triggers on release");

    // Drag beyond the threshold: no draggable ancestor, the tentative owner keeps the capture
    ResetGui();
    gButtonClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 50 }, true, true, false);
    DrawSingleButton();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 80, 50 }, true, false, false);
    DrawSingleButton();
    End();
    CHECK(GuiGetCapturedId() == 1, "drag with no direction owner keeps the tentative capture");

    Begin(RAYGUI_CLITERAL(Vector2){ 80, 50 }, false, false, true);
    DrawSingleButton();
    End();
    CHECK(gButtonClicks == 1, "button still triggers when dragged beyond the threshold");
}

static void TestReleaseOutsideCancels(void)
{
    ResetGui();
    gButtonClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 50 }, true, true, false);
    DrawSingleButton();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, true);
    DrawSingleButton();
    End();

    CHECK(gButtonClicks == 0, "discrete control released outside its bounds is cancelled");

    // Dragging outside and releasing outside must also cancel
    ResetGui();
    gButtonClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 50 }, true, true, false);
    DrawSingleButton();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, true, false, false);
    DrawSingleButton();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, true);
    DrawSingleButton();
    End();

    CHECK(gButtonClicks == 0, "discrete control dragged and released outside is cancelled");
}

static void TestContinuousDragOutside(void)
{
    ResetGui();
    gSliderValue = 0.5f;
    gSliderResult = RESULT_NONE;

    Begin(RAYGUI_CLITERAL(Vector2){ 100, 15 }, true, true, false);
    DrawSliderOnly();
    End();

    // Drag past the right edge of the slider, then release outside
    Begin(RAYGUI_CLITERAL(Vector2){ 260, 15 }, true, false, false);
    DrawSliderOnly();
    End();
    CHECK(gSliderValue > 0.9f, "slider keeps dragging outside its bounds");

    Begin(RAYGUI_CLITERAL(Vector2){ 260, 15 }, false, false, true);
    DrawSliderOnly();
    End();
    CHECK(gSliderResult == RESULT_NONE, "continuous drag release outside does not repeat a result");
}

static void TestSliderThreshold(void)
{
    ResetGui();
    gSliderValue = 0.5f;
    gSliderResult = RESULT_NONE;

    Begin(RAYGUI_CLITERAL(Vector2){ 100, 15 }, true, true, false);
    DrawSliderOnly();
    End();
    CHECK(gSliderValue == 0.5f, "slider does not change value on the press frame");

    Begin(RAYGUI_CLITERAL(Vector2){ 160, 15 }, true, false, false);
    DrawSliderOnly();
    End();
    CHECK(GuiGetCapturedId() == 1, "slider captures after crossing the drag threshold");
    CHECK(gSliderValue > 0.5f, "slider updates value after the drag threshold");
    CHECK(gSliderResult == RESULT_CHANGED, "slider returns RESULT_CHANGED when value changes");

    float valueOnRelease = gSliderValue;
    gSliderResult = RESULT_NONE;
    Begin(RAYGUI_CLITERAL(Vector2){ 160, 15 }, false, false, true);
    DrawSliderOnly();
    End();
    CHECK(gSliderValue == valueOnRelease, "slider does not apply value again on release");
    CHECK(gSliderResult == RESULT_NONE, "slider does not repeat a result on release");
}

static void TestSliderQuickClick(void)
{
    ResetGui();
    gSliderValue = 0.5f;
    gSliderResult = RESULT_NONE;

    PressRelease(RAYGUI_CLITERAL(Vector2){ 170, 15 }, DrawSliderOnly);

    CHECK(gSliderValue > 0.7f, "quick click applies slider position on release");
    CHECK(gSliderResult == RESULT_CHANGED, "quick click returns RESULT_CHANGED");
}

static void TestDirectionArbitration(void)
{
    ResetGui();
    gSliderValue = 0.5f;
    gSliderResult = RESULT_NONE;
    gOverlayButtonClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 100, 15 }, true, true, false);
    DrawSliderWithButton();
    End();
    CHECK(GuiGetCapturedId() == 2, "overlay button is tentative on press");

    Begin(RAYGUI_CLITERAL(Vector2){ 160, 15 }, true, false, false);
    DrawSliderWithButton();
    End();
    CHECK(GuiGetCapturedId() == 1, "horizontal drag transfers capture to the slider under the button");
    CHECK(gOverlayButtonClicks == 0, "overlay button does not trigger when arbitration transfers capture");

    Begin(RAYGUI_CLITERAL(Vector2){ 160, 15 }, false, false, true);
    DrawSliderWithButton();
    End();
    CHECK(gOverlayButtonClicks == 0, "overlay button stays cancelled after release");
}

static void TestHoverOwnership(void)
{
    ResetGui();
    gLowerClicks = 0;
    gUpperClicks = 0;

    // First idle frame builds the hit stack, hover is resolved at frame end
    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, false, false, false);
    DrawOverlappingButtons();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, false, false, false);
    DrawOverlappingButtons();
    End();
    CHECK(guiCapture.hoverId == 2, "hover only targets the topmost hit control");

    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawOverlappingButtons();
    End();
    CHECK(guiCapture.hoverIdNext == 0, "hover clears when the pointer leaves every control");
}

static void TestTextBoxCommit(void)
{
    ResetGui();
    strcpy(gTextBoxText, "abc");
    gTextBoxResult = RESULT_NONE;

    // Press inside starts editing
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, true, true, false);
    DrawTextBoxOnly();
    End();
    CHECK(GuiIsEditing(), "press inside enters text edit mode");
    CHECK(gTextBoxResult == RESULT_NONE, "entering edit mode does not return a result");

    // Release inside keeps editing, no result
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, true);
    DrawTextBoxOnly();
    End();
    CHECK(GuiIsEditing(), "release inside keeps edit mode");
    CHECK(gTextBoxResult == RESULT_NONE, "focus click does not return a commit");

    // ENTER commits immediately
    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawTextBoxOnly();
    End();
    gKeyPressed = 0;
    CHECK(!GuiIsEditing(), "ENTER exits text edit mode");
    CHECK(gTextBoxResult == RESULT_PRESSED, "ENTER delivers the commit result");
    CHECK(strcmp(gTextBoxText, "abc") == 0, "text buffer is preserved by the GUI");
}

static void TestTextBoxOutsideCommit(void)
{
    ResetGui();
    strcpy(gTextBoxText, "abc");
    gTextBoxResult = RESULT_NONE;
    gUnderlayClicks = 0;

    // Enter edit mode
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, true, true, false);
    DrawTextBoxOverButton();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, true);
    DrawTextBoxOverButton();
    End();
    CHECK(GuiIsEditing(), "text box is editing before the outside click");

    // Outside press exits edit mode; the underlying button must not react
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, true, true, false);
    DrawTextBoxOverButton();
    End();
    CHECK(!GuiIsEditing(), "outside press exits edit mode immediately");
    CHECK(gUnderlayClicks == 0, "underlying button does not react on the outside press frame");

    // Release delivers the commit and still must not trigger the button
    gTextBoxResult = RESULT_NONE;
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, true);
    DrawTextBoxOverButton();
    End();
    CHECK(gTextBoxResult == RESULT_PRESSED, "outside click delivers the commit on release");
    CHECK(gUnderlayClicks == 0, "underlying button does not react on the outside release frame");
}

static void TestValueBox(void)
{
    ResetGui();
    gValue = 10;
    gValueResult = RESULT_NONE;

    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, true, true, false);
    DrawValueBoxOnly();
    End();
    CHECK(GuiIsEditing(), "value box press enters edit mode");

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    gKeyPressed = 0;
    CHECK(!GuiIsEditing(), "value box ENTER exits edit mode");
    CHECK(gValueResult == RESULT_PRESSED, "value box ENTER delivers a commit result");
}

static void TestValueBoxParseOnCommit(void)
{
    ResetGui();
    gValue = 10;
    gValueResult = RESULT_NONE;

    // Enter edit mode
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawValueBoxOnly);
    CHECK(GuiIsEditing(), "value box is editing");

    // Typing changes only the text, not the value
    gCharPressed = '5';
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    gCharPressed = 0;
    CHECK(gValue == 10, "value is not parsed while typing");

    // ENTER commits and parses, clamping to the configured maximum
    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    gKeyPressed = 0;

    CHECK(gValue == 100, "value is parsed and clamped on commit");
    CHECK(gValueResult == RESULT_PRESSED, "commit result is returned after parsing");

    // Invalid text keeps the previous value
    ResetGui();
    gValue = 10;
    gValueResult = RESULT_NONE;
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawValueBoxOnly);

    gKeyPressed = KEY_BACKSPACE;
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    gKeyPressed = 0;

    gCharPressed = 'x';
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    gCharPressed = 0;

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, false);
    DrawValueBoxOnly();
    End();
    gKeyPressed = 0;

    CHECK(gValue == 10, "invalid text keeps the previous value on commit");
}

static float gFloatValue = 1.5f;
static int gFloatResult = RESULT_NONE;

static void DrawFloatValueBoxOnly(void)
{
    gFloatResult = GuiValueBoxFloat(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL, &gFloatValue, 0.0f, 0.0f);
}

static void DrawFloatValueBoxRange(void)
{
    gFloatResult = GuiValueBoxFloat(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL, &gFloatValue, 0.0f, 2.0f);
}

static void TestValueBoxFloat(void)
{
    ResetGui();
    gFloatValue = 1.5f;
    gFloatResult = RESULT_NONE;

    PressRelease(RAYGUI_CLITERAL(Vector2){ 110, 10 }, DrawFloatValueBoxOnly);
    CHECK(GuiIsEditing(), "float value box enters edit mode");

    gCharPressed = '2';
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxOnly();
    End();
    gCharPressed = 0;

    CHECK(gFloatValue == 1.5f, "float value is not parsed while typing");

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxOnly();
    End();
    gKeyPressed = 0;

    CHECK(gFloatValue == 1.52f, "float value is parsed on commit");
    CHECK(gFloatResult == RESULT_PRESSED, "float value box returns the commit result");

    // Invalid text keeps the previous value
    PressRelease(RAYGUI_CLITERAL(Vector2){ 110, 10 }, DrawFloatValueBoxOnly);

    for (int i = 0; i < 4; i++)
    {
        gKeyPressed = KEY_BACKSPACE;
        Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
        DrawFloatValueBoxOnly();
        End();
    }
    gKeyPressed = 0;

    gCharPressed = 'x';
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxOnly();
    End();
    gCharPressed = 0;

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxOnly();
    End();
    gKeyPressed = 0;

    CHECK(gFloatValue == 1.52f, "invalid float text keeps the previous value");
}

static int gUnboundedValue = 500;

static void DrawUnboundedValueBox(void)
{
    gValueResult = GuiValueBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL, &gUnboundedValue, 0, 0);
}

static void TestValueBoxLimits(void)
{
    ResetGui();
    gFloatValue = 5.0f;

    // maxValue > minValue: external values and commits are clamped
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxRange();
    End();
    CHECK(gFloatValue == 2.0f, "float value box clamps external values when a range is set");

    gFloatValue = 1.0f;
    PressRelease(RAYGUI_CLITERAL(Vector2){ 110, 10 }, DrawFloatValueBoxRange);

    gCharPressed = '5';
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxRange();
    End();
    gCharPressed = 0;

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawFloatValueBoxRange();
    End();
    gKeyPressed = 0;

    CHECK(gFloatValue == 2.0f, "float value box clamps on commit");

    // maxValue <= minValue: no range limit for int ValueBox
    ResetGui();
    gUnboundedValue = 500;
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawUnboundedValueBox();
    End();
    CHECK(gUnboundedValue == 500, "value box keeps out-of-range values when maxValue <= minValue");

    PressRelease(RAYGUI_CLITERAL(Vector2){ 110, 10 }, DrawUnboundedValueBox);
    gCharPressed = '1';
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawUnboundedValueBox();
    End();
    gCharPressed = 0;

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 110, 10 }, false, false, false);
    DrawUnboundedValueBox();
    End();
    gKeyPressed = 0;

    CHECK(gUnboundedValue == 5001, "value box does not clamp on commit when maxValue <= minValue");
}

static void TestDropdownModal(void)
{
    ResetGui();
    gDropdownActive = 0;
    gDropdownResult = RESULT_NONE;
    gUnderDropdownClicks = 0;

    // Open on release
    PressRelease(RAYGUI_CLITERAL(Vector2){ 50, 15 }, DrawDropdownOverButton);
    CHECK((guiCapture.dropdownOpenId != 0), "dropdown opens on release");
    CHECK(gDropdownResult == RESULT_PRESSED, "dropdown open returns RESULT_PRESSED");

    // Select the second item
    int spacing = GuiGetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING);
    float itemY = 0.0f + (1 + 1)*(30.0f + spacing) + 15.0f;
    PressRelease(RAYGUI_CLITERAL(Vector2){ 50, itemY }, DrawDropdownOverButton);
    CHECK(gDropdownActive == 1, "dropdown selects the item under the pointer");
    CHECK((guiCapture.dropdownOpenId == 0), "dropdown closes after selection");
    CHECK(gDropdownResult == RESULT_CHANGED, "dropdown selection returns RESULT_CHANGED");
    CHECK(gUnderDropdownClicks == 0, "underlying button is not triggered by the dropdown");

    // Outside click closes the dropdown without activating the button underneath
    PressRelease(RAYGUI_CLITERAL(Vector2){ 50, 15 }, DrawDropdownOverButton);
    CHECK((guiCapture.dropdownOpenId != 0), "dropdown reopens");

    PressRelease(RAYGUI_CLITERAL(Vector2){ 300, 300 }, DrawDropdownOverButton);
    CHECK((guiCapture.dropdownOpenId == 0), "outside click closes the dropdown");
    CHECK(gUnderDropdownClicks == 0, "outside click does not activate the underlying button");
}

static void TestDropdownLockedModal(void)
{
    ResetGui();
    gDropdownActive = 0;
    gUnderDropdownClicks = 0;

    // Open normally, then lock the UI: the open popup must freeze as well
    PressRelease(RAYGUI_CLITERAL(Vector2){ 50, 15 }, DrawDropdownOverButton);
    CHECK((guiCapture.dropdownOpenId != 0), "dropdown is open before locking");

    GuiLock();
    PressRelease(RAYGUI_CLITERAL(Vector2){ 300, 300 }, DrawDropdownOverButton);
    CHECK((guiCapture.dropdownOpenId != 0), "locked dropdown stays open and ignores clicks");
    CHECK(gUnderDropdownClicks == 0, "locked dropdown does not activate the underlying button");
    GuiUnlock();

    // After unlocking the normal outside click closes it
    PressRelease(RAYGUI_CLITERAL(Vector2){ 300, 300 }, DrawDropdownOverButton);
    CHECK((guiCapture.dropdownOpenId == 0), "dropdown closes after unlocking");
    CHECK(gUnderDropdownClicks == 0, "unlocked dropdown does not activate the underlying button");
}

static void TestLockBlocksContinuousAndTextControls(void)
{
    ResetGui();
    gSliderValue = 0.5f;
    gSliderResult = RESULT_NONE;

    // Locked slider cannot be dragged
    GuiLock();
    Begin(RAYGUI_CLITERAL(Vector2){ 100, 15 }, true, true, false);
    DrawSliderOnly();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 170, 15 }, true, false, false);
    DrawSliderOnly();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 170, 15 }, false, false, true);
    DrawSliderOnly();
    End();
    GuiUnlock();

    CHECK(gSliderValue == 0.5f, "locked slider does not react to dragging");
    CHECK(gSliderResult == RESULT_NONE, "locked slider returns no result");

    // Locked text box cannot enter edit mode
    ResetGui();
    strcpy(gTextBoxText, "abc");
    GuiLock();
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawTextBoxOnly);
    GuiUnlock();

    CHECK(!GuiIsEditing(), "locked text box does not enter edit mode");

    // Unlocked, the same click works
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawTextBoxOnly);
    CHECK(GuiIsEditing(), "text box edits normally after unlocking");
}

static void TestComboBoxSelector(void)
{
    ResetGui();
    gComboActive = 0;
    gComboResult = RESULT_NONE;

    int bw = GuiGetStyle(COMBOBOX, COMBO_BUTTON_WIDTH);
    int sp = GuiGetStyle(COMBOBOX, COMBO_BUTTON_SPACING);
    float selectorX = 0.0f + (120.0f - bw - sp) + sp + bw/2.0f;

    PressRelease(RAYGUI_CLITERAL(Vector2){ selectorX, 15 }, DrawComboBox);

    CHECK(gComboActive == 1, "clicking the combo selector still cycles the value");
    CHECK(gComboResult == RESULT_CHANGED, "combo selector returns RESULT_CHANGED");
}

static void TestScrollPanelWheel(void)
{
    ResetGui();
    gScroll = RAYGUI_CLITERAL(Vector2){ 0, 0 };
    gScrollResult = RESULT_NONE;

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 100 }, false, false, false);
    gWheel = -1.0f;
    DrawScrollPanel();
    End();
    gWheel = 0.0f;

    CHECK(gScroll.y < 0.0f, "mouse wheel scrolls the panel");
    CHECK(gScrollResult == RESULT_CHANGED, "scroll panel returns RESULT_CHANGED when scroll changes");

    // Locked panel does not scroll
    gScroll = RAYGUI_CLITERAL(Vector2){ 0, 0 };
    GuiLock();
    Begin(RAYGUI_CLITERAL(Vector2){ 50, 100 }, false, false, false);
    gWheel = -1.0f;
    DrawScrollPanel();
    End();
    gWheel = 0.0f;
    GuiUnlock();

    CHECK(gScroll.y == 0.0f, "locked scroll panel does not scroll with the wheel");
}

static void TestScrollPanelContentDrag(void)
{
    ResetGui();
    gScroll = RAYGUI_CLITERAL(Vector2){ 0, 0 };
    gScrollResult = RESULT_NONE;

    // Press on empty content (not on a scrollbar)
    Begin(RAYGUI_CLITERAL(Vector2){ 50, 100 }, true, true, false);
    DrawScrollPanel();
    End();

    // Drag content up-left: this scrolls down-right
    Begin(RAYGUI_CLITERAL(Vector2){ 30, 70 }, true, false, false);
    DrawScrollPanel();
    End();

    CHECK(gScroll.x < 0.0f, "dragging content pans the scroll panel horizontally");
    CHECK(gScroll.y < 0.0f, "dragging content pans the scroll panel vertically");
    CHECK(gScrollResult == RESULT_CHANGED, "content drag reports RESULT_CHANGED");

    Begin(RAYGUI_CLITERAL(Vector2){ 30, 70 }, false, false, true);
    DrawScrollPanel();
    End();

    CHECK(gScroll.x < 0.0f, "scroll value is kept after releasing a content drag");

    // Locked panel must not pan either
    gScroll = RAYGUI_CLITERAL(Vector2){ 0, 0 };
    GuiLock();
    Begin(RAYGUI_CLITERAL(Vector2){ 50, 100 }, true, true, false);
    DrawScrollPanel();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 30, 70 }, true, false, false);
    DrawScrollPanel();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 30, 70 }, false, false, true);
    DrawScrollPanel();
    End();
    GuiUnlock();

    CHECK((gScroll.x == 0.0f) && (gScroll.y == 0.0f), "locked scroll panel does not pan by content drag");
}

static void TestScrollPanelSlowContentDrag(void)
{
    ResetGui();
    gScroll = RAYGUI_CLITERAL(Vector2){ 0, 0 };

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 100 }, true, true, false);
    DrawScrollPanel();
    End();

    // Below the 5 px threshold: no pan yet
    Begin(RAYGUI_CLITERAL(Vector2){ 47, 97 }, true, false, false);
    DrawScrollPanel();
    End();
    CHECK((gScroll.x == 0.0f) && (gScroll.y == 0.0f), "content drag does not pan before the threshold");

    // Crossing the threshold must include the pre-threshold movement (total -7,-7)
    Begin(RAYGUI_CLITERAL(Vector2){ 43, 93 }, true, false, false);
    DrawScrollPanel();
    End();
    CHECK((gScroll.x == -7.0f) && (gScroll.y == -7.0f), "first active drag applies the whole gesture delta");

    Begin(RAYGUI_CLITERAL(Vector2){ 43, 93 }, false, false, true);
    DrawScrollPanel();
    End();
}

static void TestGridDoesNotTrigger(void)
{
    ResetGui();
    gGridResult = RESULT_PRESSED;
    gGridCell = RAYGUI_CLITERAL(Vector2){ -1, -1 };

    Begin(RAYGUI_CLITERAL(Vector2){ 50, 50 }, false, false, false);
    DrawGrid();
    End();

    CHECK(gGridResult == RESULT_NONE, "grid never reports a discrete result");
    CHECK(gGridCell.x == 2, "grid still reports the hovered cell");
}

static void TestModalDragKeepsCapture(void)
{
    ResetGui();
    strcpy(gTextBoxText, "abc");
    gTextBoxResult = RESULT_NONE;
    gUnderlayClicks = 0;

    // Enter edit mode
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, true, true, false);
    DrawTextBoxOverButton();
    End();
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, true);
    DrawTextBoxOverButton();
    End();

    // Outside press followed by a drag must keep the modal capture (no direction arbitration)
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, true, true, false);
    DrawTextBoxOverButton();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 360, 300 }, true, false, false);
    DrawTextBoxOverButton();
    End();
    CHECK(GuiGetCapturedId() == 2, "modal keeps the capture after dragging outside");

    gTextBoxResult = RESULT_NONE;
    Begin(RAYGUI_CLITERAL(Vector2){ 360, 300 }, false, false, true);
    DrawTextBoxOverButton();
    End();
    CHECK(gTextBoxResult == RESULT_PRESSED, "modal drag still delivers the commit on release");
    CHECK(gUnderlayClicks == 0, "modal drag never activates the underlying button");
}

static char gTextBoxTextA[32] = "A";
static char gTextBoxTextB[32] = "B";
static int gTextBoxResultA = RESULT_NONE;
static int gTextBoxResultB = RESULT_NONE;

static void DrawTwoTextBoxes(void)
{
    gTextBoxResultA = GuiTextBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, gTextBoxTextA, 32);
    gTextBoxResultB = GuiTextBox(RAYGUI_CLITERAL(Rectangle){ 0, 40, 120, 30 }, gTextBoxTextB, 32);
}

static void TestTextBoxExclusiveInput(void)
{
    ResetGui();
    strcpy(gTextBoxTextA, "A");
    strcpy(gTextBoxTextB, "B");
    gTextBoxResultA = RESULT_NONE;
    gTextBoxResultB = RESULT_NONE;

    // Select the first text box
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawTwoTextBoxes);
    CHECK(GuiIsEditing(), "clicking a text box starts editing");

    // Typed characters go only to the selected text box
    gCharPressed = 'x';
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTwoTextBoxes();
    End();
    gCharPressed = 0;

    CHECK(strcmp(gTextBoxTextA, "Ax") == 0, "selected text box receives keyboard input");
    CHECK(strcmp(gTextBoxTextB, "B") == 0, "unselected text box does not receive keyboard input");

    // Select the second text box: the first one commits and stops receiving input
    gTextBoxResultA = RESULT_NONE;
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 50 }, DrawTwoTextBoxes);

    CHECK(GuiIsEditing(), "a single text box remains in edit mode after switching");
    CHECK(gTextBoxResultA == RESULT_PRESSED, "previous editor delivers its commit result");

    gCharPressed = 'y';
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTwoTextBoxes();
    End();
    gCharPressed = 0;

    CHECK(strcmp(gTextBoxTextA, "Ax") == 0, "previous editor no longer receives keyboard input");
    CHECK(strcmp(gTextBoxTextB, "By") == 0, "new editor receives keyboard input");
}

static int gSpinnerValue = 10;
static int gSpinnerResult = RESULT_NONE;

static void DrawSpinnerOnly(void)
{
    gSpinnerResult = GuiSpinner(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL, &gSpinnerValue, 0, 100);
}

static void TestSpinnerValueBox(void)
{
    ResetGui();
    gSpinnerValue = 10;
    gSpinnerResult = RESULT_NONE;

    int buttonWidth = GuiGetStyle(VALUEBOX, SPINNER_BUTTON_WIDTH);
    Vector2 middle = { 60, 15 };
    Vector2 rightArrow = { 120.0f - buttonWidth/2.0f, 15 };

    // Middle area enters edit mode through the internal value box
    PressRelease(middle, DrawSpinnerOnly);
    CHECK(GuiIsEditing(), "spinner middle area enters edit mode via its internal value box");

    // ENTER commits the internal value box
    gKeyPressed = KEY_ENTER;
    Begin(middle, false, false, false);
    DrawSpinnerOnly();
    End();
    gKeyPressed = 0;
    CHECK(!GuiIsEditing(), "spinner ENTER exits edit mode");
    CHECK(gSpinnerResult == RESULT_PRESSED, "spinner ENTER delivers the commit result");

    // Arrow click steps the value when not editing
    int before = gSpinnerValue;
    PressRelease(rightArrow, DrawSpinnerOnly);
    CHECK(gSpinnerValue == before + 1, "spinner arrow steps the value when not editing");

    // Arrow steps are clamped to the configured range
    gSpinnerValue = 100;
    PressRelease(rightArrow, DrawSpinnerOnly);
    CHECK(gSpinnerValue == 100, "spinner arrow is clamped to the maximum value");
}

static void TestSpinnerArrowWhileEditing(void)
{
    ResetGui();
    gSpinnerValue = 10;
    gSpinnerResult = RESULT_NONE;

    int buttonWidth = GuiGetStyle(VALUEBOX, SPINNER_BUTTON_WIDTH);
    Vector2 middle = { 60, 15 };
    Vector2 rightArrow = { 120.0f - buttonWidth/2.0f, 15 };

    // Enter edit mode, then click the arrow while editing
    PressRelease(middle, DrawSpinnerOnly);
    CHECK(GuiIsEditing(), "spinner is editing before the arrow click");

    int before = gSpinnerValue;
    PressRelease(rightArrow, DrawSpinnerOnly);

    // The arrow is a separate control: the modal value box only commits on this click
    CHECK(!GuiIsEditing(), "arrow click while editing commits the spinner");
    CHECK(gSpinnerValue == before, "the first arrow click only commits, it does not step");
    CHECK(gSpinnerResult == RESULT_PRESSED, "arrow click while editing returns the commit result");

    // The next arrow click works normally
    before = gSpinnerValue;
    PressRelease(rightArrow, DrawSpinnerOnly);
    CHECK(gSpinnerValue == before + 1, "spinner arrow steps after the commit");
}

static void DrawTextBoxAndSpinner(void)
{
    gTextBoxResultA = GuiTextBox(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, gTextBoxTextA, 32);
    gSpinnerResult = GuiSpinner(RAYGUI_CLITERAL(Rectangle){ 0, 40, 120, 30 }, NULL, &gSpinnerValue, 0, 100);
}

static void TestSpinnerTakesFocusFromTextBox(void)
{
    ResetGui();
    gTextBoxTextA[0] = '\0';
    gTextBoxResultA = RESULT_NONE;
    gSpinnerValue = 10;

    // Focus the text box first
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawTextBoxAndSpinner);
    CHECK(GuiIsEditing(), "text box is editing before clicking the spinner");

    // Clicking the spinner middle commits the text box and moves the focus
    PressRelease(RAYGUI_CLITERAL(Vector2){ 60, 55 }, DrawTextBoxAndSpinner);
    CHECK(gTextBoxResultA == RESULT_PRESSED, "previous text editor delivers its commit");
    CHECK(GuiIsEditing(), "spinner takes the edit focus from the text box");

    // Keyboard input now goes to the spinner, not to the committed text box
    int spinnerValue = gSpinnerValue;
    int textLength = (int)strlen(gTextBoxTextA);

    gCharPressed = '7';
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTextBoxAndSpinner();
    End();
    gCharPressed = 0;

    // ValueBox parses only on commit
    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTextBoxAndSpinner();
    End();
    gKeyPressed = 0;

    CHECK((int)strlen(gTextBoxTextA) == textLength, "committed text box no longer receives input");
    CHECK(gSpinnerValue != spinnerValue, "spinner receives the keyboard input");
}

static int gSpinnerValueA = 10;
static int gSpinnerValueB = 20;

static void DrawTwoSpinners(void)
{
    GuiSpinner(RAYGUI_CLITERAL(Rectangle){ 0, 0, 120, 30 }, NULL, &gSpinnerValueA, 0, 100);
    GuiSpinner(RAYGUI_CLITERAL(Rectangle){ 0, 40, 120, 30 }, NULL, &gSpinnerValueB, 0, 100);
}

static void TestTwoSpinnersExclusiveInput(void)
{
    ResetGui();
    gSpinnerValueA = 10;
    gSpinnerValueB = 20;

    // Select the first spinner
    PressRelease(RAYGUI_CLITERAL(Vector2){ 60, 15 }, DrawTwoSpinners);
    CHECK(GuiIsEditing(), "clicking a spinner starts editing");

    gCharPressed = '5';
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTwoSpinners();
    End();
    gCharPressed = 0;

    // ValueBox parses only on commit
    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTwoSpinners();
    End();
    gKeyPressed = 0;

    CHECK(gSpinnerValueA != 10, "active spinner receives keyboard input");
    CHECK(gSpinnerValueB == 20, "inactive spinner does not receive keyboard input");

    // Clicking the second spinner commits the first one and moves the focus
    PressRelease(RAYGUI_CLITERAL(Vector2){ 60, 55 }, DrawTwoSpinners);
    CHECK(GuiIsEditing(), "clicking another spinner keeps a single editor active");

    // The first spinner has been committed and clamped; from now on it must stay unchanged
    int valueA = gSpinnerValueA;

    gCharPressed = '7';
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTwoSpinners();
    End();
    gCharPressed = 0;

    gKeyPressed = KEY_ENTER;
    Begin(RAYGUI_CLITERAL(Vector2){ 300, 300 }, false, false, false);
    DrawTwoSpinners();
    End();
    gKeyPressed = 0;

    CHECK(gSpinnerValueA == valueA, "committed spinner no longer receives keyboard input");
    CHECK(gSpinnerValueB != 20, "newly focused spinner receives keyboard input");
}

static void TestOneDiscreteTriggerPerFrame(void)
{
    ResetGui();
    gLowerClicks = 0;
    gUpperClicks = 0;
    gButtonClicks = 0;

    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, true, true, false);
    DrawOverlappingButtons();
    End();

    Begin(RAYGUI_CLITERAL(Vector2){ 60, 50 }, false, false, true);
    // Two independent controls under the pointer: only the captured one may return
    DrawOverlappingButtons();
    End();

    CHECK((gUpperClicks == 1) && (gLowerClicks == 0), "only the captured control returns a discrete result");
}

//----------------------------------------------------------------------------------
// Phase 2: explicit input, contexts and routing
//----------------------------------------------------------------------------------

static int gCtxClicksA = 0;
static int gCtxClicksB = 0;
static int gCtxSliderResult = RESULT_NONE;
static float gCtxSliderValue = 0.0f;

static void DrawCtxButtonA(void)
{
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 10, 10, 60, 30 }, "A")) gCtxClicksA++;
}

static void DrawCtxButtonB(void)
{
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 210, 10, 60, 30 }, "B")) gCtxClicksB++;
}

static void DrawCtxLeftSlider(void)
{
    gCtxSliderResult = GuiSlider(RAYGUI_CLITERAL(Rectangle){ 10, 10, 80, 20 }, NULL, NULL, &gCtxSliderValue, 0.0f, 1.0f);
}

static void DrawCtxRightButton(void)
{
    if (GuiButton(RAYGUI_CLITERAL(Rectangle){ 120, 10, 60, 30 }, "B")) gCtxClicksB++;
}

static void DrawCtxRightSlider(void)
{
    gCtxSliderResult = GuiSlider(RAYGUI_CLITERAL(Rectangle){ 120, 10, 80, 20 }, NULL, NULL, &gCtxSliderValue, 0.0f, 1.0f);
}

static void DrawTwoContexts(GuiContext *left, GuiContext *right, bool slider)
{
    GuiPushContext(left, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    if (slider) DrawCtxLeftSlider();
    else DrawCtxButtonA();
    GuiPopContext();

    GuiPushContext(right, RAYGUI_CLITERAL(Rectangle){ 100, 0, 200, 100 });
    DrawCtxRightButton();
    GuiPopContext();
}

static void TestExplicitInputClick(void)
{
    ResetGui();
    gButtonClicks = 0;

    GuiInput input = { { 10, 10 }, true, false, true, 0 };
    GuiBegin(&input, 1);
    DrawSingleButton();
    GuiEnd();

    input.pressed = false;
    GuiBegin(&input, 1);
    DrawSingleButton();
    GuiEnd();

    input.released = true;
    input.down = false;
    GuiBegin(&input, 1);
    DrawSingleButton();
    GuiEnd();

    CHECK(gButtonClicks == 1, "GuiBegin() click is delivered on release");
}

static void TestMultiContextIndependentTouches(void)
{
    ResetGui();
    GuiContext left = { 0 };
    GuiContext right = { 0 };
    gCtxClicksA = 0;
    gCtxClicksB = 0;
    gCtxSliderValue = 0.2f;

    GuiInput inputs[2] = {
        { { 20, 20 }, true, false, true, 0 },
        { { 140, 20 }, true, false, true, 1 }
    };

    GuiBegin(inputs, 2);
    DrawTwoContexts(&left, &right, false);
    GuiEnd();

    inputs[0].pressed = false;
    inputs[1].pressed = false;
    GuiBegin(inputs, 2);
    DrawTwoContexts(&left, &right, false);
    GuiEnd();

    inputs[0].down = false;
    inputs[0].released = true;
    inputs[1].down = false;
    inputs[1].released = true;
    GuiBegin(inputs, 2);
    DrawTwoContexts(&left, &right, false);
    GuiEnd();

    CHECK(gCtxClicksA == 1, "left context receives its own touch");
    CHECK(gCtxClicksB == 1, "right context receives its own touch");
}

static void TestContextCaptureStaysRouted(void)
{
    ResetGui();
    GuiContext left = { 0 };
    GuiContext right = { 0 };
    gCtxClicksA = 0;
    gCtxClicksB = 0;
    gCtxSliderValue = 0.2f;

    GuiInput inputs[1] = { { { 20, 15 }, true, false, true, 0 } };

    GuiBegin(inputs, 1);
    DrawTwoContexts(&left, &right, true);
    GuiEnd();

    // Move outside the left context bounds but keep holding the touch
    inputs[0].position = RAYGUI_CLITERAL(Vector2){ 150, 15 };
    inputs[0].pressed = false;
    GuiBegin(inputs, 1);
    DrawTwoContexts(&left, &right, true);
    GuiEnd();

    CHECK(gCtxSliderValue > 0.2f, "context keeps updating while the touch leaves its bounds");

    inputs[0].down = false;
    inputs[0].released = true;
    GuiBegin(inputs, 1);
    DrawTwoContexts(&left, &right, true);
    GuiEnd();

    CHECK(gCtxClicksB == 0, "a captured touch is not rerouted to another context");
}

static void TestContextCaptureNotStolenByEarlierPush(void)
{
    ResetGui();
    GuiContext left = { 0 };
    GuiContext right = { 0 };
    gCtxClicksA = 0;
    gCtxSliderValue = 0.2f;

    // The right context captures the touch first
    GuiInput input = { { 150, 15 }, true, false, true, 0 };

    GuiBegin(&input, 1);
    GuiPushContext(&right, RAYGUI_CLITERAL(Rectangle){ 100, 0, 200, 100 });
    DrawCtxRightSlider();
    GuiPopContext();
    GuiEnd();

    // Next frame the touch moves into the left bounds. Left is pushed first, but must not steal it.
    input.position = RAYGUI_CLITERAL(Vector2){ 50, 15 };
    input.pressed = false;

    GuiBegin(&input, 1);
    GuiPushContext(&left, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxButtonA();
    GuiPopContext();

    GuiPushContext(&right, RAYGUI_CLITERAL(Rectangle){ 100, 0, 200, 100 });
    DrawCtxRightSlider();
    GuiPopContext();
    GuiEnd();

    input.down = false;
    input.released = true;
    GuiBegin(&input, 1);
    GuiPushContext(&left, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxButtonA();
    GuiPopContext();

    GuiPushContext(&right, RAYGUI_CLITERAL(Rectangle){ 100, 0, 200, 100 });
    DrawCtxRightSlider();
    GuiPopContext();
    GuiEnd();

    CHECK(gCtxClicksA == 0, "a routed touch is not stolen by an earlier sibling context");
    CHECK(gCtxSliderValue != 0.2f, "the owning context keeps updating the touch");
}

static void TestCancelAndReleaseCapture(void)
{
    ResetGui();
    gButtonClicks = 0;

    // Press then cancel: release must not trigger
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, true, true, false);
    DrawSingleButton();
    End();
    GuiCancel(NULL);

    CHECK(GuiGetCapturedId() == 0, "GuiCancel() clears the capture");

    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, true);
    DrawSingleButton();
    End();

    CHECK(gButtonClicks == 0, "GuiCancel() prevents the release from triggering");

    // Press then release capture explicitly: release must not trigger either
    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, true, true, false);
    DrawSingleButton();
    End();
    GuiReleaseCapture(NULL);

    Begin(RAYGUI_CLITERAL(Vector2){ 10, 10 }, false, false, true);
    DrawSingleButton();
    End();

    CHECK(gButtonClicks == 0, "GuiReleaseCapture() prevents the release from triggering");

    // Cancel also clears text editing, leaving the buffer untouched
    ResetGui();
    strcpy(gTextBoxText, "abc");
    PressRelease(RAYGUI_CLITERAL(Vector2){ 10, 10 }, DrawTextBoxOnly);
    CHECK(GuiIsEditing(), "text box is editing before cancel");

    GuiCancel(NULL);
    CHECK(!GuiIsEditing(), "GuiCancel() clears the text editor");
    CHECK(strcmp(gTextBoxText, "abc") == 0, "GuiCancel() leaves the text buffer untouched");
}

static void TestDragThresholdConfig(void)
{
    ResetGui();
    gSliderValue = 0.2f;
    gSliderResult = RESULT_NONE;

    float previous = GuiGetDragThreshold();
    GuiSetDragThreshold(20.0f);
    CHECK(GuiGetDragThreshold() == 20.0f, "drag threshold can be queried");

    Begin(RAYGUI_CLITERAL(Vector2){ 20, 15 }, true, true, false);
    DrawSliderOnly();
    End();

    // 10 px movement is below the configured 20 px threshold
    Begin(RAYGUI_CLITERAL(Vector2){ 30, 15 }, true, false, false);
    DrawSliderOnly();
    End();

    CHECK(gSliderValue == 0.2f, "movement below the configured threshold does not drag");

    Begin(RAYGUI_CLITERAL(Vector2){ 30, 15 }, false, false, true);
    DrawSliderOnly();
    End();

    GuiSetDragThreshold(previous);
}

static void TestDefaultContextIgnoresExtraInputs(void)
{
    ResetGui();
    gCtxClicksA = 0;

    GuiInput inputs[2] = {
        { { 10, 60 }, true, false, true, 0 },
        { { 20, 20 }, true, false, true, 1 }
    };

    GuiBegin(inputs, 2);
    DrawCtxButtonA();
    GuiEnd();

    inputs[0].pressed = false;
    inputs[1].pressed = false;
    GuiBegin(inputs, 2);
    DrawCtxButtonA();
    GuiEnd();

    inputs[0].down = false;
    inputs[0].released = true;
    inputs[1].down = false;
    inputs[1].released = true;
    GuiBegin(inputs, 2);
    DrawCtxButtonA();
    GuiEnd();

    CHECK(gCtxClicksA == 0, "default context only uses the first input");
}

static void TestContextCancelInsideFrame(void)
{
    ResetGui();
    GuiContext ctx = { 0 };
    gCtxClicksA = 0;

    GuiInput input = { { 20, 20 }, true, false, true, 0 };

    GuiBegin(&input, 1);
    GuiPushContext(&ctx, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxButtonA();
    GuiPopContext();

    GuiCancel(&ctx);

    // The same context can be pushed again in the same frame; it must not corrupt the frame list
    GuiPushContext(&ctx, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxButtonA();
    GuiPopContext();
    GuiEnd();

    CHECK(gCtxClicksA == 0, "cancelled press does not deliver a click");
}

static void TestContextMissingRelease(void)
{
    ResetGui();
    GuiContext ctx = { 0 };
    gCtxSliderValue = 0.2f;

    GuiInput input = { { 20, 15 }, true, false, true, 0 };

    GuiBegin(&input, 1);
    GuiPushContext(&ctx, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxLeftSlider();
    GuiPopContext();
    GuiEnd();

    // No input at all: the routed touch is treated as released instead of sticking forever
    GuiBegin(NULL, 0);
    GuiPushContext(&ctx, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxLeftSlider();
    GuiPopContext();
    GuiEnd();

    GuiBegin(NULL, 0);
    GuiPushContext(&ctx, RAYGUI_CLITERAL(Rectangle){ 0, 0, 100, 100 });
    DrawCtxLeftSlider();
    CHECK(GuiGetCapturedId() == 0, "missing touch release clears the context capture");
    GuiPopContext();
    GuiEnd();
}

int main(void)
{
    GuiLoadStyleDefault();

    TestTopmostWins();
    TestEmptyPressDoesNotClick();
    TestThresholdClickAndDrag();
    TestReleaseOutsideCancels();
    TestContinuousDragOutside();
    TestSliderThreshold();
    TestSliderQuickClick();
    TestDirectionArbitration();
    TestHoverOwnership();
    TestTextBoxCommit();
    TestTextBoxOutsideCommit();
    TestModalDragKeepsCapture();
    TestTextBoxExclusiveInput();
    TestValueBox();
    TestValueBoxParseOnCommit();
    TestValueBoxFloat();
    TestValueBoxLimits();
    TestSpinnerValueBox();
    TestSpinnerArrowWhileEditing();
    TestSpinnerTakesFocusFromTextBox();
    TestTwoSpinnersExclusiveInput();
    TestDropdownModal();
    TestDropdownLockedModal();
    TestLockBlocksContinuousAndTextControls();
    TestComboBoxSelector();
    TestScrollPanelWheel();
    TestScrollPanelContentDrag();
    TestScrollPanelSlowContentDrag();
    TestGridDoesNotTrigger();
    TestOneDiscreteTriggerPerFrame();
    TestExplicitInputClick();
    TestMultiContextIndependentTouches();
    TestContextCaptureStaysRouted();
    TestContextCaptureNotStolenByEarlierPush();
    TestCancelAndReleaseCapture();
    TestDragThresholdConfig();
    TestDefaultContextIgnoresExtraInputs();
    TestContextCancelInsideFrame();
    TestContextMissingRelease();

    printf("%d checks, %d failures\n", gChecks, gFailures);
    return (gFailures == 0)? 0 : 1;
}
