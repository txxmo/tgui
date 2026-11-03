cmake_minimum_required( VERSION 3.16 )
project( tg LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

option( TG_BUILD_EXAMPLES "Build the GLFW/OpenGL showcase" ON )
option( TG_BUILD_HEADLESS "Build the headless software-rendered example/test" ON )

# ---------------------------------------------------------------- core library
add_library( tg STATIC
    src/context.cpp
    src/draw.cpp
    src/font.cpp
    src/theme.cpp
    src/widget.cpp
    src/widgets/box.cpp
    src/widgets/button.cpp
    src/widgets/chart.cpp
    src/widgets/checkbox.cpp
    src/widgets/image.cpp
    src/widgets/label.cpp
    src/widgets/progress.cpp
    src/widgets/scrollview.cpp
    src/widgets/slider.cpp
    src/widgets/textinput.cpp
    src/widgets/toggle.cpp
)
target_include_directories( tg PUBLIC include PRIVATE third_party/stb )
if( MSVC )
    target_compile_options( tg PRIVATE /W4 /utf-8 )
    target_compile_definitions( tg PRIVATE _CRT_SECURE_NO_WARNINGS )
else()
    target_compile_options( tg PRIVATE -Wall -Wextra )
endif()

# ---------------------------------------------------------------- backends
add_library( tg_soft STATIC backends/soft_renderer.cpp )
target_include_directories( tg_soft PUBLIC backends PRIVATE third_party/stb )
target_link_libraries( tg_soft PUBLIC tg )

set( TG_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" )

# ---------------------------------------------------------------- examples
if( TG_BUILD_HEADLESS )
    add_executable( tg_headless examples/headless/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_headless PRIVATE tg_soft )
    target_compile_definitions( tg_headless PRIVATE TG_ASSET_DIR="${TG_ASSET_DIR}" )
    enable_testing()
    add_test( NAME headless COMMAND tg_headless ${CMAKE_CURRENT_BINARY_DIR} )
endif()

if( TG_BUILD_EXAMPLES )
    find_package( glfw3 3.3 QUIET )
    if( NOT glfw3_FOUND )
        include( FetchContent )
        set( GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE )
        set( GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE )
        set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
        FetchContent_Declare( glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE )
        FetchContent_MakeAvailable( glfw )
    endif()

    add_library( tg_gl3 STATIC backends/gl3_renderer.cpp backends/glfw_platform.cpp )
    target_include_directories( tg_gl3 PUBLIC backends )
    target_link_libraries( tg_gl3 PUBLIC tg glfw )

    add_executable( tg_showcase examples/showcase/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_showcase PRIVATE tg_gl3 )
    target_compile_definitions( tg_showcase PRIVATE TG_ASSET_DIR="${TG_ASSET_DIR}" )
    if( WIN32 )
        set_target_properties( tg_showcase PROPERTIES WIN32_EXECUTABLE OFF )
    endif()
endif()
