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
        // Build this frame's input explicitly; a custom backend can feed any source
        GuiInput input = {
            GetMousePosition(),
            IsMouseButtonPressed(0),
            IsMouseButtonReleased(0),
            IsMouseButtonDown(0),
            0
        };

        // Start a gui frame (automatic ids and capture state are rebuilt every frame)
        GuiBegin(&input, 1);

        // TODO: Use raygui API

        GuiEnd();
    }
    
    // TODO: De-initialize all resources

    return 0;
}
