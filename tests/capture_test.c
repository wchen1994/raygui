/*******************************************************************************************
*
*   raygui - Control capture (capture id) headless tests
*
*   Uses RAYGUI_STANDALONE with a deterministic fake input backend to verify:
*       - topmost control wins the capture on press
*       - lower controls do not react on the press frame (including sliders/scrollbars)
*       - only the captured id can interact until release
*       - click-like results are returned on release, not on press
*       - a press on empty space blocks the UI until release
*       - untagged controls keep the legacy behavior
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include <stdio.h>
#include <string.h>

#define RAYGUI_STANDALONE

// Fake input is provided through the standalone backend functions; these macros
// simply route raygui inputs to them (they are expanded inside the implementation)
#define GUI_BUTTON_DOWN         IsMouseButtonDown(0)
#define GUI_BUTTON_PRESSED      IsMouseButtonPressed(0)
#define GUI_BUTTON_RELEASED     IsMouseButtonReleased(0)
#define GUI_SCROLL_DELTA        GetMouseWheelMove()
#define GUI_POINTER_POSITION    GetMousePosition()

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

//----------------------------------------------------------------------------------
// Fake input state, driven by the test frame helper
//----------------------------------------------------------------------------------
static Vector2 gMouse = { 0, 0 };
static bool gMouseDown = false;
static bool gMousePressed = false;
static bool gMouseReleased = false;

//----------------------------------------------------------------------------------
// Fake raylib backend (standalone mode)
//----------------------------------------------------------------------------------
static Rectangle gFontRecs[256] = { 0 };
static GlyphInfo gFontGlyphs[256] = { 0 };

static Vector2 GetMousePosition(void) { return gMouse; }
static float GetMouseWheelMove(void) { return 0.0f; }
static bool IsMouseButtonDown(int button) { (void)button; return gMouseDown; }
static bool IsMouseButtonPressed(int button) { (void)button; return gMousePressed; }
static bool IsMouseButtonReleased(int button) { (void)button; return gMouseReleased; }
static bool IsKeyDown(int key) { (void)key; return false; }
static bool IsKeyPressed(int key) { (void)key; return false; }
static int GetCharPressed(void) { return 0; }
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

static void Frame(Vector2 pos, bool down, bool pressed, bool released)
{
    gMouse = pos;
    gMouseDown = down;
    gMousePressed = pressed;
    gMouseReleased = released;
}

static void Idle(void) { Frame((Vector2){ 0, 0 }, false, false, false); }

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------
static void TestUntaggedCompatibility(void)
{
    // NOTE: Must run before any non-zero capture id is assigned, capture is opt-in
    bool clicked = false;

    for (int frame = 0; frame < 4; frame++)
    {
        Idle();
        if (frame == 1) Frame((Vector2){ 50, 50 }, true, true, false);
        if (frame == 2) Frame((Vector2){ 50, 50 }, true, false, false);
        if (frame == 3) Frame((Vector2){ 50, 50 }, false, false, true);

        if (GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL)) clicked = true;
    }

    CHECK(clicked, "untagged button still works with capture disabled");
}

static void TestTopmostWins(void)
{
    int lowerClicks = 0;
    int upperClicks = 0;

    Idle();
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL)) lowerClicks++;   // drawn first (bottom)
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 50, 0, 100, 100 }, NULL)) upperClicks++;  // drawn last (topmost)
    GuiSetCapturedId(0);

    // Press frame: no control returns a result and no side effect is applied
    Frame((Vector2){ 75, 50 }, true, true, false);
    lowerClicks = 0; upperClicks = 0;
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL)) lowerClicks++;
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 50, 0, 100, 100 }, NULL)) upperClicks++;
    GuiSetCapturedId(0);

    CHECK(lowerClicks == 0, "lower control does not react on press");
    CHECK(upperClicks == 0, "upper control does not react on press");
    CHECK(GuiGetCapturedId() == 2, "topmost id captured on press");

    // Hold frame
    Frame((Vector2){ 75, 50 }, true, false, false);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL)) lowerClicks++;
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 50, 0, 100, 100 }, NULL)) upperClicks++;
    GuiSetCapturedId(0);

    CHECK(lowerClicks == 0, "lower control locked while upper captured");

    // Release frame
    Frame((Vector2){ 75, 50 }, false, false, true);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL)) lowerClicks++;
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 50, 0, 100, 100 }, NULL)) upperClicks++;
    GuiSetCapturedId(0);

    CHECK(lowerClicks == 0, "lower control does not fire on release");
    CHECK(upperClicks == 1, "topmost control fires on release");
    CHECK(GuiGetCapturedId() == 2, "captured id kept during release frame");

    Idle();
    GuiSetCapturedId(1);
    GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL);
    GuiSetCapturedId(0);
    CHECK(GuiGetCapturedId() == 0, "capture cleared on idle frame");
}

static void TestLowerSliderDoesNotSteal(void)
{
    float sliderValue = 0.5f;
    int buttonClicks = 0;

    // Slider under the button; both overlap around (100, 0)-(160, 30)
    Idle();
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    GuiButton((Rectangle){ 100, 0, 60, 30 }, NULL);
    GuiSetCapturedId(0);

    // Press on the floating button
    Frame((Vector2){ 130, 15 }, true, true, false);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 100, 0, 60, 30 }, NULL)) buttonClicks++;
    GuiSetCapturedId(0);

    CHECK(sliderValue == 0.5f, "slider value unchanged on press");
    CHECK(GuiGetCapturedId() == 2, "floating button captured over slider");

    // Hold frame
    Frame((Vector2){ 130, 15 }, true, false, false);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 100, 0, 60, 30 }, NULL)) buttonClicks++;
    GuiSetCapturedId(0);

    CHECK(sliderValue == 0.5f, "slider value unchanged while button held");

    // Release frame
    Frame((Vector2){ 130, 15 }, false, false, true);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    if (GuiButton((Rectangle){ 100, 0, 60, 30 }, NULL)) buttonClicks++;
    GuiSetCapturedId(0);

    CHECK(buttonClicks == 1, "floating button fires on release");
    CHECK(sliderValue == 0.5f, "slider value still unchanged after release");
}

static void TestEmptySpaceBlocks(void)
{
    int clicks = 0;

    Idle();
    GuiSetCapturedId(1);
    GuiButton((Rectangle){ 100, 100, 50, 50 }, NULL);
    GuiSetCapturedId(0);

    // Press on empty space, drag over the button, release there
    Frame((Vector2){ 10, 10 }, true, true, false);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 100, 100, 50, 50 }, NULL)) clicks++;
    GuiSetCapturedId(0);

    Frame((Vector2){ 120, 120 }, true, false, false);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 100, 100, 50, 50 }, NULL)) clicks++;
    GuiSetCapturedId(0);
    CHECK(GuiGetCapturedId() == 0, "no id captured when pressing empty space");

    Frame((Vector2){ 120, 120 }, false, false, true);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 100, 100, 50, 50 }, NULL)) clicks++;
    GuiSetCapturedId(0);

    CHECK(clicks == 0, "press on empty space blocks the control until release");
}

static void TestSliderDrag(void)
{
    float sliderValue = 0.0f;

    Idle();
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);

    // Press at the middle, then drag right
    Frame((Vector2){ 100, 15 }, true, true, false);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    CHECK(sliderValue == 0.0f, "slider does not jump on press");
    CHECK(GuiGetCapturedId() == 1, "slider captured on press");

    Frame((Vector2){ 150, 15 }, true, false, false);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    CHECK(sliderValue > 0.5f, "slider starts dragging on the held frame");

    Frame((Vector2){ 180, 15 }, true, false, false);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    CHECK(sliderValue > 0.8f, "slider keeps dragging while held");

    Frame((Vector2){ 180, 15 }, false, false, true);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    CHECK(GuiGetCapturedId() == 1, "capture kept during slider release");
}

static void TestReleaseOutsideCancels(void)
{
    int clicks = 0;

    Idle();
    GuiSetCapturedId(1);
    GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL);
    GuiSetCapturedId(0);

    Frame((Vector2){ 25, 25 }, true, true, false);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL)) clicks++;
    GuiSetCapturedId(0);

    Frame((Vector2){ 200, 200 }, true, false, false);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL)) clicks++;
    GuiSetCapturedId(0);

    Frame((Vector2){ 200, 200 }, false, false, true);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL)) clicks++;
    GuiSetCapturedId(0);

    CHECK(clicks == 0, "release outside bounds cancels the click");
}

static void TestClickResultsOnRelease(void)
{
    int comboActive = 0;
    char textBoxText[32] = "abc";
    int textBoxResultOnPress = 0;
    int textBoxResultOnRelease = 0;
    int listActive = -1;
    int listScroll = 0;

    // ComboBox: cycle on release
    Idle();
    GuiSetCapturedId(10);
    GuiComboBox((Rectangle){ 0, 0, 120, 30 }, "A;B;C", &comboActive);
    GuiSetCapturedId(0);
    CHECK(comboActive == 0, "combo not changed on idle");

    Frame((Vector2){ 60, 15 }, true, true, false);
    GuiSetCapturedId(10);
    GuiComboBox((Rectangle){ 0, 0, 120, 30 }, "A;B;C", &comboActive);
    GuiSetCapturedId(0);
    CHECK(comboActive == 0, "combo does not cycle on press");

    Frame((Vector2){ 60, 15 }, false, false, true);
    GuiSetCapturedId(10);
    GuiComboBox((Rectangle){ 0, 0, 120, 30 }, "A;B;C", &comboActive);
    GuiSetCapturedId(0);
    CHECK(comboActive == 1, "combo cycles on release");

    // TextBox: enter edit on release
    Idle();
    GuiSetCapturedId(11);
    GuiTextBox((Rectangle){ 200, 0, 120, 30 }, textBoxText, 32, false);
    GuiSetCapturedId(0);

    Frame((Vector2){ 260, 15 }, true, true, false);
    GuiSetCapturedId(11);
    if (GuiTextBox((Rectangle){ 200, 0, 120, 30 }, textBoxText, 32, false) != RESULT_NONE) textBoxResultOnPress++;
    GuiSetCapturedId(0);
    CHECK(textBoxResultOnPress == 0, "textbox does not start editing on press");

    Frame((Vector2){ 260, 15 }, false, false, true);
    GuiSetCapturedId(11);
    if (GuiTextBox((Rectangle){ 200, 0, 120, 30 }, textBoxText, 32, false) != RESULT_NONE) textBoxResultOnRelease++;
    GuiSetCapturedId(0);
    CHECK(textBoxResultOnRelease == 1, "textbox starts editing on release");

    // ListView: select on release
    Idle();
    GuiSetCapturedId(12);
    GuiListView((Rectangle){ 400, 0, 120, 120 }, "A;B;C", &listScroll, &listActive);
    GuiSetCapturedId(0);

    Frame((Vector2){ 460, 20 }, true, true, false);
    GuiSetCapturedId(12);
    GuiListView((Rectangle){ 400, 0, 120, 120 }, "A;B;C", &listScroll, &listActive);
    GuiSetCapturedId(0);
    CHECK(listActive == -1, "list does not select on press");

    Frame((Vector2){ 460, 20 }, false, false, true);
    GuiSetCapturedId(12);
    GuiListView((Rectangle){ 400, 0, 120, 120 }, "A;B;C", &listScroll, &listActive);
    GuiSetCapturedId(0);
    CHECK(listActive == 0, "list selects on release");

    // ValueBox: start editing on release
    int value = 7;
    int valueBoxResultOnPress = 0;
    int valueBoxResultOnRelease = 0;

    Idle();
    GuiSetCapturedId(13);
    GuiValueBox((Rectangle){ 600, 0, 120, 30 }, NULL, &value, 0, 100, false);
    GuiSetCapturedId(0);

    Frame((Vector2){ 660, 15 }, true, true, false);
    GuiSetCapturedId(13);
    if (GuiValueBox((Rectangle){ 600, 0, 120, 30 }, NULL, &value, 0, 100, false) != RESULT_NONE) valueBoxResultOnPress++;
    GuiSetCapturedId(0);
    CHECK(valueBoxResultOnPress == 0, "valuebox does not start editing on press");

    Frame((Vector2){ 660, 15 }, false, false, true);
    GuiSetCapturedId(13);
    if (GuiValueBox((Rectangle){ 600, 0, 120, 30 }, NULL, &value, 0, 100, false) != RESULT_NONE) valueBoxResultOnRelease++;
    GuiSetCapturedId(0);
    CHECK(valueBoxResultOnRelease == 1, "valuebox starts editing on release");
}

static void TestUncapturedControlCannotSteal(void)
{
    int delayedClicks = 0;

    Idle();
    GuiSetCapturedId(1);
    GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL);
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    GuiButton((Rectangle){ 60, 0, 100, 100 }, NULL);
    GuiSetCapturedId(0);

    Frame((Vector2){ 80, 50 }, true, true, false);
    GuiSetCapturedId(1);
    GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL);
    GuiSetCapturedId(0);
    GuiSetCapturedId(2);
    GuiButton((Rectangle){ 60, 0, 100, 100 }, NULL);
    GuiSetCapturedId(0);

    // Reorder drawing while held: id 1 is now the topmost hit, but capture belongs to id 2
    Frame((Vector2){ 80, 50 }, true, false, false);
    GuiSetCapturedId(2);
    GuiButton((Rectangle){ 60, 0, 100, 100 }, NULL);
    GuiSetCapturedId(0);
    GuiSetCapturedId(1);
    if (GuiButton((Rectangle){ 0, 0, 100, 100 }, NULL)) delayedClicks++;
    GuiSetCapturedId(0);

    CHECK(delayedClicks == 0, "capture is stable to draw-order changes while held");
}

static void TestDropdownModalWithLock(void)
{
    int active = 0;
    int resultOnPress = 0;
    int resultOnRelease = 0;

    Idle();
    GuiSetCapturedId(20);
    GuiLock();

    // Open dropdown must keep working while the rest of the UI is locked
    Frame((Vector2){ 10, 10 }, true, true, false);
    GuiSetCapturedId(20);
    if (GuiDropdownBox((Rectangle){ 0, 0, 120, 30 }, "A;B;C", &active, true) != RESULT_NONE) resultOnPress++;
    GuiSetCapturedId(0);
    CHECK(resultOnPress == 0, "open dropdown does not close on press");

    Frame((Vector2){ 10, 10 }, false, false, true);
    GuiSetCapturedId(20);
    if (GuiDropdownBox((Rectangle){ 0, 0, 120, 30 }, "A;B;C", &active, true) != RESULT_NONE) resultOnRelease++;
    GuiSetCapturedId(0);

    GuiUnlock();
    CHECK(resultOnRelease == 1, "open dropdown closes on release even when GuiLock is active");
}

static void TestUntaggedFallbackAfterOptIn(void)
{
    int clicks = 0;

    Idle();

    // Capture is enabled at this point, but a control without id keeps the legacy behavior
    Frame((Vector2){ 25, 25 }, true, true, false);
    GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL);

    Frame((Vector2){ 25, 25 }, true, false, false);
    GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL);

    Frame((Vector2){ 25, 25 }, false, false, true);
    if (GuiButton((Rectangle){ 0, 0, 50, 50 }, NULL)) clicks++;

    CHECK(clicks == 1, "untagged control still works after capture is enabled");
}

static void TestSliderQuickClick(void)
{
    float sliderValue = 0.0f;

    Idle();
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);

    // Press and release with no held frame (very quick click)
    Frame((Vector2){ 150, 15 }, true, true, false);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    CHECK(sliderValue == 0.0f, "quick click does not jump on press");

    Frame((Vector2){ 150, 15 }, false, false, true);
    GuiSetCapturedId(1);
    GuiSlider((Rectangle){ 0, 0, 200, 30 }, NULL, NULL, &sliderValue, 0.0f, 1.0f);
    GuiSetCapturedId(0);
    CHECK(sliderValue > 0.5f, "quick click applies the slider value on release");
}

static void TestColorBarQuickClick(void)
{
    float alpha = 0.0f;

    Idle();
    GuiSetCapturedId(1);
    GuiColorBarAlpha((Rectangle){ 0, 0, 200, 30 }, NULL, &alpha);
    GuiSetCapturedId(0);

    Frame((Vector2){ 150, 15 }, true, true, false);
    GuiSetCapturedId(1);
    GuiColorBarAlpha((Rectangle){ 0, 0, 200, 30 }, NULL, &alpha);
    GuiSetCapturedId(0);
    CHECK(alpha == 0.0f, "color bar does not change on press");

    Frame((Vector2){ 150, 15 }, false, false, true);
    GuiSetCapturedId(1);
    GuiColorBarAlpha((Rectangle){ 0, 0, 200, 30 }, NULL, &alpha);
    GuiSetCapturedId(0);
    CHECK(alpha > 0.5f, "color bar quick click applies value on release");
}

int main(void)
{
    // Initialize default style (uses the fake font backend)
    GuiLoadStyleDefault();

    TestUntaggedCompatibility();
    TestTopmostWins();
    TestLowerSliderDoesNotSteal();
    TestEmptySpaceBlocks();
    TestSliderDrag();
    TestSliderQuickClick();
    TestColorBarQuickClick();
    TestReleaseOutsideCancels();
    TestClickResultsOnRelease();
    TestUncapturedControlCannotSteal();
    TestDropdownModalWithLock();
    TestUntaggedFallbackAfterOptIn();

    printf("%d checks, %d failures\n", gChecks, gFailures);
    return (gFailures == 0)? 0 : 1;
}
