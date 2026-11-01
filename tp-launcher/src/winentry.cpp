// winentry.cpp - WinMain shim for the GUI-subsystem (WIN32) launcher build.
//
// A /SUBSYSTEM:WINDOWS executable on MSVC needs WinMain, not main. Qt ships
// an entry-point library for this, but Qt 6.8 exports it only as the internal
// Qt6::EntryPointPrivate (there is no Qt6::EntryPoint target), so the launcher
// carries the four lines itself and calls the ordinary main() in main.cpp.
//
// Only compiled on Windows: on Linux the launcher links as a console app.
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdlib>

int main(int argc, char* argv[]);

extern "C" int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}

#endif  // _WIN32
