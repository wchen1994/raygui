/*******************************************************************************************
*
*   raygui - Standalone mode usage template
*
*   DEPENDENCIES:
*       raygui 2.6  - Immediate-mode GUI controls.
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2020 Ramon Santamaria (@raysan5)
*
**********************************************************************************************/

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_STANDALONE
#include "../../src/raygui.h"

#include "raygui_custom_backend.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // TODO: Initialize your systems (window, graphics, inputs)
    
    // TODO: Create your game loop
    {
        // One context is one pointer; keep it alive across frames. The host decides
        // which pointer goes to it and where the pointer state comes from.
        static GuiContext guiContext = { 0 };
        guiContext.pointerPosition = GetMousePosition();
        guiContext.pointerPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        guiContext.pointerReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
        guiContext.pointerDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        // Start a gui frame (automatic ids and capture state are rebuilt every frame)
        GuiBegin(&guiContext);

        // TODO: Use raygui API

        GuiEnd();

        // GuiEnd writes back whether a control held this pointer
        if (guiContext.pointerCaptured) { /* TODO: keep pointer away from game input */ }
    }
    
    // TODO: De-initialize all resources

    return 0;
}
