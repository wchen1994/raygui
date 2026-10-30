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

#include "custom_backend.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // TODO: Initialize your systems (window, graphics, inputs)
    
    // TODO: Create your game loop
    {
        // Start a gui frame (automatic ids and capture state are rebuilt every frame)
        GuiBeginFrame();

        // TODO: Use raygui API

        GuiEndFrame();
    }
    
    // TODO: De-initialize all resources

    return 0;
}
