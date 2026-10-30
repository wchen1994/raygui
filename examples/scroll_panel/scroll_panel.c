/*******************************************************************************************
*
*   raygui - Controls test
*
*   TEST CONTROLS:
*       - GuiScrollPanel()
*       - GuiSetCapturedId() / GuiPushId() / GuiPopId() control capture
*
*   DEPENDENCIES:
*       raylib 6.1-dev      - Windowing/input management and drawing
*       raygui 5.0-dev      - Immediate-mode GUI controls with custom styling and icons
*
*   COMPILATION (Windows - MinGW):
*       gcc -o $(NAME_PART).exe $(FILE_NAME) -I../../src -lraylib -lopengl32 -lgdi32 -std=c99
*
*   COMPILATION (Linux - gcc):
*       gcc -o $(NAME_PART) $(FILE_NAME) -I../../src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -std=c99
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2019-2026 Vlad Adrian (@Demizdor) and Ramon Santamaria (@raysan5)
*
**********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "../../src/raygui.h"

// Capture ids must be stable for the whole press-to-release period.
// The grid uses a fixed stride so ids do not shift when the column count changes.
enum {
    CAPTURE_ID_SCROLL_PANEL      = 1,
    CAPTURE_ID_GRID_BASE         = 1000,
    CAPTURE_ID_GRID_STRIDE       = 100,
    CAPTURE_ID_FLOATING_BUTTON   = 9000,
    CAPTURE_ID_STYLE_BASE        = 20000,
    CAPTURE_ID_SHOW_CONTENT      = 30000,
    CAPTURE_ID_WIDTH             = 30001,
    CAPTURE_ID_HEIGHT            = 30002
};

static void DrawStyleEditControls(void);  // Draw and process scroll bar style edition controls

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //---------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raygui - GuiScrollPanel()");

    Rectangle panelRec = { 20, 40, 200, 150 };
    Rectangle panelContentRec = {0, 0, 340, 340 };
    Rectangle panelView = { 0 };
    Vector2 panelScroll = { 99, -20 };

    bool showContentArea = true;

    SetTargetFPS(60);
    //---------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Implement required update logic
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText(TextFormat("[%f, %f]", panelScroll.x, panelScroll.y), 4, 4, 20, RED);

            GuiPushId(CAPTURE_ID_SCROLL_PANEL);
            GuiScrollPanel(panelRec, NULL, panelContentRec, &panelScroll, &panelView);
            GuiPopId();

            int buttonSize = 64;
            int rCnt = panelContentRec.height / buttonSize;
            int cCnt = panelContentRec.width / buttonSize;
            BeginScissorMode(panelView.x, panelView.y, panelView.width, panelView.height);
            GuiSetScissor(panelView);
                GuiGrid((Rectangle){panelRec.x + panelScroll.x, panelRec.y + panelScroll.y, panelContentRec.width, panelContentRec.height}, NULL, 16, 3, NULL);
                for (int r=0; r < rCnt; ++r)
                    for (int c = 0; c < cCnt; ++c)
                    {
                        GuiPushId(CAPTURE_ID_GRID_BASE + r*CAPTURE_ID_GRID_STRIDE + c);
                        if (GuiButton((Rectangle) { panelRec.x + panelScroll.x + c * buttonSize, panelRec.y + panelScroll.y + r * buttonSize, buttonSize, buttonSize }, ""))
                        {
                            TraceLog(LOG_INFO, "%d,%d", r, c);
                        }
                        GuiPopId();
                    }

                // Drawn after the grid: it overlays the grid buttons and must win the capture
                GuiPushId(CAPTURE_ID_FLOATING_BUTTON);
                if (GuiButton((Rectangle) { panelRec.x + panelScroll.x + 0.5 * buttonSize, panelRec.y + panelScroll.y + 0.5 * buttonSize, buttonSize, buttonSize }, ""))
                {
                    TraceLog(LOG_INFO, "floating button");
                }
                GuiPopId();
            GuiSetScissor((Rectangle) { 0 });
            EndScissorMode();

            if (showContentArea) DrawRectangle(panelRec.x + panelScroll.x, panelRec.y + panelScroll.y, panelContentRec.width, panelContentRec.height, Fade(RED, 0.1));

            DrawStyleEditControls();

            GuiPushId(CAPTURE_ID_SHOW_CONTENT);
            GuiCheckBox((Rectangle){ 565, 80, 20, 20 }, "SHOW CONTENT AREA", &showContentArea);
            GuiPopId();

            GuiPushId(CAPTURE_ID_WIDTH);
            GuiSliderBar((Rectangle){ 590, 385, 145, 15}, "WIDTH", TextFormat("%i", (int)panelContentRec.width), &panelContentRec.width, 1, 600);
            GuiPopId();

            GuiPushId(CAPTURE_ID_HEIGHT);
            GuiSliderBar((Rectangle){ 590, 410, 145, 15 }, "HEIGHT", TextFormat("%i", (int)panelContentRec.height), &panelContentRec.height, 1, 400);
            GuiPopId();

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

// Draw and process scroll bar style edition controls
static void DrawStyleEditControls(void)
{
    // The layout of this panel is static, so assigning ids in call order is stable.
    // For dynamic layouts, derive ids from the control identity instead (see the grid above).
    int captureId = CAPTURE_ID_STYLE_BASE;

    // ScrollPanel style controls
    //----------------------------------------------------------
    GuiGroupBox((Rectangle){ 550, 170, 220, 205 }, "SCROLLBAR STYLE");

    int style = GuiGetStyle(SCROLLBAR, BORDER_WIDTH);
    GuiLabel((Rectangle){ 555, 195, 110, 10 }, "BORDER_WIDTH");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 190, 90, 20 }, NULL, &style, 0, 6, false);
    GuiSetStyle(SCROLLBAR, BORDER_WIDTH, style);

    style = GuiGetStyle(SCROLLBAR, ARROWS_SIZE);
    GuiLabel((Rectangle){ 555, 220, 110, 10 }, "ARROWS_SIZE");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 215, 90, 20 }, NULL, &style, 4, 14, false);
    GuiSetStyle(SCROLLBAR, ARROWS_SIZE, style);

    style = GuiGetStyle(SCROLLBAR, SLIDER_PADDING);
    GuiLabel((Rectangle){ 555, 245, 110, 10 }, "SLIDER_PADDING");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 240, 90, 20 }, NULL, &style, 0, 14, false);
    GuiSetStyle(SCROLLBAR, SLIDER_PADDING, style);

    bool scrollBarArrows = GuiGetStyle(SCROLLBAR, ARROWS_VISIBLE);
    GuiSetCapturedId(captureId++);
    GuiCheckBox((Rectangle){ 565, 280, 20, 20 }, "ARROWS_VISIBLE", &scrollBarArrows);
    GuiSetStyle(SCROLLBAR, ARROWS_VISIBLE, scrollBarArrows);

    style = GuiGetStyle(SCROLLBAR, SLIDER_PADDING);
    GuiLabel((Rectangle){ 555, 325, 110, 10 }, "SLIDER_PADDING");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 320, 90, 20 }, NULL, &style, 0, 14, false);
    GuiSetStyle(SCROLLBAR, SLIDER_PADDING, style);

    style = GuiGetStyle(SCROLLBAR, SLIDER_WIDTH);
    GuiLabel((Rectangle){ 555, 350, 110, 10 }, "SLIDER_WIDTH");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 345, 90, 20 }, NULL, &style, 2, 100, false);
    GuiSetStyle(SCROLLBAR, SLIDER_WIDTH, style);

    const char *text = GuiGetStyle(LISTVIEW, SCROLLBAR_SIDE) == SCROLLBAR_LEFT_SIDE? "SCROLLBAR: LEFT" : "SCROLLBAR: RIGHT";
    bool toggleScrollBarSide = GuiGetStyle(LISTVIEW, SCROLLBAR_SIDE);
    GuiSetCapturedId(captureId++);
    GuiToggle((Rectangle){ 560, 110, 200, 35 }, text, &toggleScrollBarSide);
    GuiSetStyle(LISTVIEW, SCROLLBAR_SIDE, toggleScrollBarSide);
    //----------------------------------------------------------

    // ScrollBar style controls
    //----------------------------------------------------------
    GuiGroupBox((Rectangle){ 550, 20, 220, 135 }, "SCROLLPANEL STYLE");

    style = GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH);
    GuiLabel((Rectangle){ 555, 35, 110, 10 }, "SCROLLBAR_WIDTH");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 30, 90, 20 }, NULL, &style, 6, 30, false);
    GuiSetStyle(LISTVIEW, SCROLLBAR_WIDTH, style);

    style = GuiGetStyle(DEFAULT, BORDER_WIDTH);
    GuiLabel((Rectangle){ 555, 60, 110, 10 }, "BORDER_WIDTH");
    GuiSetCapturedId(captureId++);
    GuiSpinner((Rectangle){ 670, 55, 90, 20 }, NULL, &style, 0, 20, false);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, style);
    //----------------------------------------------------------

    GuiSetCapturedId(0);
}
