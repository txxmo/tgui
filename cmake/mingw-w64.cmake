# Cross-compile the Windows build from Linux with MinGW-w64:
#   cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
# Executables are statically linked so they run on a stock Windows (or Wine) with no MinGW DLLs.
set( CMAKE_SYSTEM_NAME Windows )
set( CMAKE_SYSTEM_PROCESSOR x86_64 )
set( CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix )
set( CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix )
set( CMAKE_RC_COMPILER x86_64-w64-mingw32-windres )
set( CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 )
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++" )
add_compile_definitions( _WIN32_WINNT=0x0A00 ) # no UNICODE: match Visual Studio's default (ANSI) so A/W mixups show up here
