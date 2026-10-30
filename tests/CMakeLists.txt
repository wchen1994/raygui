add_executable(capture_test capture_test.c)
target_include_directories(capture_test PRIVATE ${RAYGUI_SRC})
set_target_properties(capture_test PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

if(MSVC)
    target_compile_definitions(capture_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_test(NAME capture_test COMMAND capture_test)
