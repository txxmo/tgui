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
option( TG_HARFBUZZ       "Shape emoji sequences (flags, ZWJ families, skin tones) with HarfBuzz; implies TG_FREETYPE" OFF )
if( TG_HARFBUZZ AND NOT TG_FREETYPE )
    message( STATUS "tg: TG_HARFBUZZ needs FreeType for color glyphs, turning TG_FREETYPE on" )
    set( TG_FREETYPE ON CACHE BOOL "" FORCE )
endif()
option( TG_WEBVIEW        "Build tg::webview: web pages inside tg (WebView2 / WebKitGTK / WKWebView)" OFF )
option( TG_BUILD_EXAMPLES "Build hello + showcase (needs TG_BUILD_APP)" ${TG_TOP_LEVEL} )
option( TG_BUILD_HEADLESS "Build the headless software-rendered test"   ${TG_TOP_LEVEL} )

# ---------------------------------------------------------------- core library (no platform deps)
add_library( tg STATIC
    src/binds.cpp
    src/devtools.cpp
    src/history.cpp
    src/icons.cpp
    src/settings.cpp
    src/svg.cpp
    src/context.cpp
    src/default_fonts.cpp
    src/draw.cpp
    src/effects.cpp
    src/font.cpp
    src/input.cpp
    src/pty.cpp
    src/theme.cpp
    src/variants.cpp
    src/widget.cpp
    src/widgets/bars.cpp
    src/widgets/box.cpp
    src/widgets/button.cpp
    src/widgets/chart.cpp
    src/widgets/charts.cpp
    src/widgets/checkbox.cpp
    src/widgets/choice.cpp
    src/widgets/codeeditor.cpp
    src/widgets/collapsible.cpp
    src/widgets/colorpicker.cpp
    src/widgets/combo.cpp
    src/widgets/container.cpp
    src/widgets/datepicker.cpp
    src/widgets/dialog.cpp
    src/widgets/dials.cpp
    src/widgets/extras.cpp
    src/widgets/groupbox.cpp
    src/widgets/image.cpp
    src/widgets/imageviewer.cpp
    src/widgets/keybind.cpp
    src/widgets/label.cpp
    src/widgets/listbox.cpp
    src/widgets/listitem.cpp
    src/widgets/menu.cpp
    src/widgets/menubar.cpp
    src/widgets/number.cpp
    src/widgets/palette.cpp
    src/widgets/popup.cpp
    src/widgets/propertygrid.cpp
    src/widgets/progress.cpp
    src/widgets/rangeslider.cpp
    src/widgets/richtext.cpp
    src/widgets/scrollview.cpp
    src/widgets/searchbox.cpp
    src/widgets/slider.cpp
    src/widgets/steps.cpp
    src/widgets/splitter.cpp
    src/widgets/swatches.cpp
    src/widgets/table.cpp
    src/widgets/tabview.cpp
    src/widgets/taginput.cpp
    src/widgets/terminal.cpp
    src/widgets/textarea.cpp
    src/widgets/textinput.cpp
    src/widgets/themeeditor.cpp
    src/widgets/timepicker.cpp
    src/widgets/toast.cpp
    src/widgets/toggle.cpp
    src/widgets/treeview.cpp
    src/widgets/virtuallist.cpp
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

if( TG_HARFBUZZ )
    find_package( harfbuzz CONFIG QUIET )               # harfbuzz::harfbuzz (Linux distro packages, vcpkg)
    if( NOT TARGET harfbuzz::harfbuzz )
        find_package( PkgConfig QUIET )
        if( PKG_CONFIG_FOUND AND NOT CMAKE_CROSSCOMPILING )   # host .pc files are wrong for a cross build
            pkg_check_modules( HARFBUZZ QUIET IMPORTED_TARGET harfbuzz )
            if( HARFBUZZ_FOUND )
                add_library( harfbuzz::harfbuzz ALIAS PkgConfig::HARFBUZZ )
            endif()
        endif()
    endif()
    if( NOT TARGET harfbuzz::harfbuzz )
        # Not installed (typical on Windows): build it. tg only needs the core shaper, no FreeType/ICU/glib glue.
        include( FetchContent )
        set( HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE )
        set( HB_HAVE_GLIB OFF CACHE BOOL "" FORCE )
        set( HB_HAVE_ICU OFF CACHE BOOL "" FORCE )
        set( HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE )
        set( HB_BUILD_UTILS OFF CACHE BOOL "" FORCE )
        FetchContent_Declare( harfbuzz GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git GIT_TAG 10.1.0 GIT_SHALLOW TRUE )
        FetchContent_MakeAvailable( harfbuzz )
        add_library( harfbuzz::harfbuzz ALIAS harfbuzz )
    endif()
    target_link_libraries( tg PUBLIC harfbuzz::harfbuzz )
    target_compile_definitions( tg PUBLIC TG_HAS_HARFBUZZ=1 )
endif()
target_include_directories( tg PUBLIC include PRIVATE third_party/stb third_party/nanosvg )
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

# ---------------------------------------------------------------- web view (optional)
# tg_copy_webview_runtime( <target> ): MinGW builds load WebView2Loader.dll at runtime; this copies it next to the exe.
function( tg_copy_webview_runtime Target )
    if( TG_WEBVIEW2_LOADER_DLL )
        add_custom_command( TARGET ${Target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            ${TG_WEBVIEW2_LOADER_DLL} $<TARGET_FILE_DIR:${Target}> )
    endif()
endfunction()

if( TG_WEBVIEW )
    add_library( tg_webview STATIC src/webview/webview.cpp )
    add_library( tg::webview ALIAS tg_webview )
    target_link_libraries( tg_webview PUBLIC tg )
    if( WIN32 )
        # WebView2 SDK from NuGet (headers + loader). The runtime itself is part of Windows.
        include( FetchContent )
        FetchContent_Declare( webview2
            URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.2903.40/microsoft.web.webview2.1.0.2903.40.nupkg
            URL_HASH SHA256=ef128016dd1e51c59178c827ed5b8aa3322c57afa8675d930f8109505542ad74
            DOWNLOAD_NAME webview2.zip )
        FetchContent_MakeAvailable( webview2 )
        set( WV2 ${webview2_SOURCE_DIR}/build/native )
        if( CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64" )
            set( WV2_ARCH arm64 )
        elseif( CMAKE_SIZEOF_VOID_P EQUAL 8 )
            set( WV2_ARCH x64 )
        else()
            set( WV2_ARCH x86 )
        endif()
        target_sources( tg_webview PRIVATE src/webview/webview_win32.cpp )
        target_include_directories( tg_webview PRIVATE ${WV2}/include )
        target_link_libraries( tg_webview PUBLIC ole32 user32 )
        if( MSVC )
            target_link_libraries( tg_webview PRIVATE ${WV2}/${WV2_ARCH}/WebView2LoaderStatic.lib )
            target_compile_definitions( tg_webview PRIVATE TG_WEBVIEW2_STATIC=1 )
        else()
            # MinGW: the header includes "EventToken.h"; MinGW ships it lowercase
            file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/tg_webview_shim/EventToken.h "#include <eventtoken.h>\n" )
            target_include_directories( tg_webview PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tg_webview_shim )
            set( TG_WEBVIEW2_LOADER_DLL ${WV2}/${WV2_ARCH}/WebView2Loader.dll CACHE INTERNAL "" )
        endif()
    elseif( APPLE )
        enable_language( OBJCXX )
        target_sources( tg_webview PRIVATE src/webview/webview_cocoa.mm )
        set_source_files_properties( src/webview/webview_cocoa.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc" )
        target_link_libraries( tg_webview PUBLIC "-framework WebKit" "-framework Cocoa" )
    elseif( UNIX )
        find_package( PkgConfig REQUIRED )
        pkg_check_modules( TG_WEBKIT IMPORTED_TARGET webkit2gtk-4.1 )
        if( NOT TG_WEBKIT_FOUND )
            pkg_check_modules( TG_WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.0 ) # older distros
        endif()
        pkg_check_modules( TG_X11 REQUIRED IMPORTED_TARGET x11 )
        target_sources( tg_webview PRIVATE src/webview/webview_gtk.cpp )
        target_link_libraries( tg_webview PRIVATE PkgConfig::TG_WEBKIT PkgConfig::TG_X11 )
        if( TARGET tg_app AND NOT TG_APP_BACKEND STREQUAL "win32" )
            target_compile_definitions( tg_app PRIVATE TG_GLFW_X11=1 ) # App: native X11 window id, X11 even under Wayland
        endif()
    else()
        target_sources( tg_webview PRIVATE src/webview/webview_none.cpp )
    endif()
endif()

# ---------------------------------------------------------------- examples
if( TG_BUILD_EXAMPLES AND TG_BUILD_APP )
    add_executable( tg_hello examples/hello/main.cpp )
    target_link_libraries( tg_hello PRIVATE tg::app )

    add_executable( tg_showcase examples/showcase/main.cpp examples/common/demo_ui.cpp )
    target_link_libraries( tg_showcase PRIVATE tg::app )
endif()

if( TG_BUILD_EXAMPLES AND TG_BUILD_APP AND TG_WEBVIEW )
    add_executable( tg_webview_demo examples/webview/main.cpp )
    target_link_libraries( tg_webview_demo PRIVATE tg::app tg::webview )
    tg_copy_webview_runtime( tg_webview_demo )
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
