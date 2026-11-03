cmake_minimum_required( VERSION 3.16 )
project( tg LANGUAGES CXX )

# Top-level build: examples on. Pulled in via add_subdirectory/FetchContent: just the libraries.
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
    set( TG_TOP_LEVEL ON )
else()
    set( TG_TOP_LEVEL OFF )
endif()

option( TG_BUILD_APP      "Build tg::app (GLFW + OpenGL 3 window/loop)" ON )
option( TG_BUILD_EXAMPLES "Build hello + showcase (needs TG_BUILD_APP)" ${TG_TOP_LEVEL} )
option( TG_BUILD_HEADLESS "Build the headless software-rendered test"   ${TG_TOP_LEVEL} )

# ---------------------------------------------------------------- core library (no platform deps)
add_library( tg STATIC
    src/context.cpp
    src/default_fonts.cpp
    src/draw.cpp
    src/font.cpp
    src/theme.cpp
    src/widget.cpp
    src/widgets/box.cpp
    src/widgets/button.cpp
    src/widgets/chart.cpp
    src/widgets/checkbox.cpp
    src/widgets/colorpicker.cpp
    src/widgets/combo.cpp
    src/widgets/container.cpp
    src/widgets/image.cpp
    src/widgets/label.cpp
    src/widgets/listbox.cpp
    src/widgets/listitem.cpp
    src/widgets/popup.cpp
    src/widgets/progress.cpp
    src/widgets/scrollview.cpp
    src/widgets/slider.cpp
    src/widgets/table.cpp
    src/widgets/textinput.cpp
    src/widgets/toggle.cpp
    src/widgets/window.cpp
)
add_library( tg::tg ALIAS tg )
target_compile_features( tg PUBLIC cxx_std_20 )
target_include_directories( tg PUBLIC include PRIVATE third_party/stb )
if( MSVC )
    target_compile_options( tg PRIVATE /W4 /utf-8 )
    target_compile_definitions( tg PRIVATE _CRT_SECURE_NO_WARNINGS )
else()
    target_compile_options( tg PRIVATE -Wall -Wextra )
endif()

# ---------------------------------------------------------------- software renderer (headless / tests)
add_library( tg_soft STATIC backends/soft_renderer.cpp )
add_library( tg::soft ALIAS tg_soft )
target_include_directories( tg_soft PUBLIC backends PRIVATE third_party/stb )
target_link_libraries( tg_soft PUBLIC tg )

# ---------------------------------------------------------------- tg::app = GLFW + OpenGL 3 + App
if( TG_BUILD_APP )
    find_package( glfw3 3.3 QUIET )
    if( NOT glfw3_FOUND )
        include( FetchContent )
        set( GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE )
        set( GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE )
        set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
        set( GLFW_INSTALL OFF CACHE BOOL "" FORCE )
        FetchContent_Declare( glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE )
        FetchContent_MakeAvailable( glfw )
    endif()

    add_library( tg_app STATIC backends/app.cpp backends/gl3_renderer.cpp backends/glfw_platform.cpp )
    add_library( tg::app ALIAS tg_app )
    target_include_directories( tg_app PUBLIC backends )
    target_link_libraries( tg_app PUBLIC tg glfw )
endif()

# ---------------------------------------------------------------- examples
if( TG_BUILD_EXAMPLES AND TG_BUILD_APP )
    add_executable( tg_hello examples/hello/main.cpp )
    target_link_libraries( tg_hello PRIVATE tg::app )

    add_executable( tg_showcase examples/showcase/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_showcase PRIVATE tg::app )
endif()

if( TG_BUILD_HEADLESS )
    add_executable( tg_headless examples/headless/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_headless PRIVATE tg::soft )
    enable_testing()
    add_test( NAME headless COMMAND tg_headless ${CMAKE_CURRENT_BINARY_DIR} )
endif()
