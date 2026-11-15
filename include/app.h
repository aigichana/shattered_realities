// app.h - the interactive live viewer (animated scene + technique pages).
#pragma once
#include <string>

// True when this platform has a live viewer (currently Windows, via Win32 GDI).
bool liveViewerAvailable();

// Opens the window and runs the animation loop until it is closed.
// Screenshots (key P) are written to outDir/screenshots.
int runLiveViewer(const std::string& outDir);
