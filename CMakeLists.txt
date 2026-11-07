cmake_minimum_required( VERSION 3.16 )
project( tg LANGUAGES CXX )

# Top-level build: examples on. Pulled in via add_subdirectory/FetchContent: just the libraries.
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
    set( TG_TOP_LEVEL ON )
else()
    set( TG_TOP_LEVEL OFF )
endif()

option( TG_BUILD_APP      "Build tg::app (GLFW + OpenGL 3 window/loop)" ON )
option( TG_FREETYPE       "Rasterize with FreeType (hinting) and enable color emoji" OFF )
option( TG_BUILD_EXAMPLES "Build hello + showcase (needs TG_BUILD_APP)" ${TG_TOP_LEVEL} )
option( TG_BUILD_HEADLESS "Build the headless software-rendered test"   ${TG_TOP_LEVEL} )

# ---------------------------------------------------------------- core library (no platform deps)
add_library( tg STATIC
    src/context.cpp
    src/default_fonts.cpp
    src/draw.cpp
    src/font.cpp
    src/input.cpp
    src/pty.cpp
    src/theme.cpp
    src/widget.cpp
    src/widgets/box.cpp
    src/widgets/button.cpp
    src/widgets/chart.cpp
    src/widgets/charts.cpp
    src/widgets/checkbox.cpp
    src/widgets/choice.cpp
    src/widgets/collapsible.cpp
    src/widgets/colorpicker.cpp
    src/widgets/combo.cpp
    src/widgets/container.cpp
    src/widgets/datepicker.cpp
    src/widgets/dialog.cpp
    src/widgets/extras.cpp
    src/widgets/groupbox.cpp
    src/widgets/image.cpp
    src/widgets/keybind.cpp
    src/widgets/label.cpp
    src/widgets/listbox.cpp
    src/widgets/listitem.cpp
    src/widgets/menu.cpp
    src/widgets/menubar.cpp
    src/widgets/number.cpp
    src/widgets/popup.cpp
    src/widgets/progress.cpp
    src/widgets/rangeslider.cpp
    src/widgets/scrollview.cpp
    src/widgets/searchbox.cpp
    src/widgets/slider.cpp
    src/widgets/splitter.cpp
    src/widgets/table.cpp
    src/widgets/tabview.cpp
    src/widgets/taginput.cpp
    src/widgets/terminal.cpp
    src/widgets/textarea.cpp
    src/widgets/textinput.cpp
    src/widgets/toast.cpp
    src/widgets/toggle.cpp
    src/widgets/treeview.cpp
    src/widgets/window.cpp
)
add_library( tg::tg ALIAS tg )
target_compile_features( tg PUBLIC cxx_std_20 )
find_package( Threads REQUIRED )
target_link_libraries( tg PUBLIC Threads::Threads )          # Pty reader threads
if( UNIX AND NOT APPLE )
    target_link_libraries( tg PUBLIC util )                  # forkpty
endif()

if( TG_FREETYPE )
    find_package( Freetype QUIET )
    if( NOT FREETYPE_FOUND )
        # No system FreeType (typical on Windows): build it. PNG support is needed for color emoji bitmaps (CBDT/sbix).
        include( FetchContent )
        set( FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE )
        set( FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE )
        set( FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE )
        FetchContent_Declare( freetype GIT_REPOSITORY https://github.com/freetype/freetype.git GIT_TAG VER-2-13-3 GIT_SHALLOW TRUE )
        FetchContent_MakeAvailable( freetype )
        add_library( Freetype::Freetype ALIAS freetype )
    endif()
    target_link_libraries( tg PUBLIC Freetype::Freetype )
    target_compile_definitions( tg PUBLIC TG_HAS_FREETYPE=1 )
endif()
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

# ---------------------------------------------------------------- Windows backends
if( WIN32 )
    add_library( tg_win32 STATIC backends/win32_platform.cpp )      # Win32 messages -> tg input
    add_library( tg::win32 ALIAS tg_win32 )
    target_include_directories( tg_win32 PUBLIC backends )
    target_link_libraries( tg_win32 PUBLIC tg )

    add_library( tg_d3d11 STATIC backends/d3d11_renderer.cpp )      # Direct3D 11 renderer
    add_library( tg::d3d11 ALIAS tg_d3d11 )
    target_include_directories( tg_d3d11 PUBLIC backends )
    target_link_libraries( tg_d3d11 PUBLIC tg d3d11 d3dcompiler )

    add_library( tg_d3d9 STATIC backends/d3d9_renderer.cpp )        # Direct3D 9 renderer (old tgui / DX9 hosts)
    add_library( tg::d3d9 ALIAS tg_d3d9 )
    target_include_directories( tg_d3d9 PUBLIC backends )
    target_link_libraries( tg_d3d9 PUBLIC tg d3d9 )
endif()

# ---------------------------------------------------------------- tg::app = window + renderer + event loop
#   win32 = native Win32 + Direct3D 11 (default on Windows, no GLFW)
#   glfw  = GLFW + OpenGL 3 (default elsewhere; also works on Windows)
if( WIN32 )
    set( TG_APP_BACKEND_DEFAULT win32 )
else()
    set( TG_APP_BACKEND_DEFAULT glfw )
endif()
set( TG_APP_BACKEND ${TG_APP_BACKEND_DEFAULT} CACHE STRING "tg::App backend: win32 or glfw" )
set_property( CACHE TG_APP_BACKEND PROPERTY STRINGS win32 glfw )

if( TG_BUILD_APP )
    if( TG_APP_BACKEND STREQUAL "win32" )
        if( NOT WIN32 )
            message( FATAL_ERROR "TG_APP_BACKEND=win32 needs a Windows target" )
        endif()
        add_library( tg_app STATIC backends/app_win32.cpp )
        target_link_libraries( tg_app PUBLIC tg_win32 tg_d3d11 dxgi )
    else()
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
        target_link_libraries( tg_app PUBLIC tg glfw )
    endif()
    add_library( tg::app ALIAS tg_app )
    target_include_directories( tg_app PUBLIC backends )
endif()

# ---------------------------------------------------------------- examples
if( TG_BUILD_EXAMPLES AND TG_BUILD_APP )
    add_executable( tg_hello examples/hello/main.cpp )
    target_link_libraries( tg_hello PRIVATE tg::app )

    add_executable( tg_showcase examples/showcase/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_showcase PRIVATE tg::app )
endif()

if( TG_BUILD_EXAMPLES AND WIN32 )
    # "Bring your own" window + device: how you'd put tg into an existing D3D app or overlay.
    add_executable( tg_win32_d3d11 examples/win32_d3d11/main.cpp )
    target_link_libraries( tg_win32_d3d11 PRIVATE tg::win32 tg::d3d11 )
    add_executable( tg_win32_d3d9 examples/win32_d3d9/main.cpp )
    target_link_libraries( tg_win32_d3d9 PRIVATE tg::win32 tg::d3d9 )
endif()

if( TG_BUILD_HEADLESS )
    add_executable( tg_headless examples/headless/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_headless PRIVATE tg::soft )
    enable_testing()
    add_test( NAME headless COMMAND tg_headless ${CMAKE_CURRENT_BINARY_DIR} )
endif()
