// main.cpp - entry point.
//   shattered_realities                 open the live viewer (Windows); elsewhere export images
//   shattered_realities --export [dir]  write every technique page and scene stage as PNG
//   shattered_realities <dir>           same as --export <dir>
#include "app.h"
#include "demos.h"
#include <cstdio>
#include <filesystem>
#include <string>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

int main(int argc, char** argv) {
    std::string out = std::string(PROJECT_ROOT) + "/output";
    bool exportMode = !liveViewerAvailable();

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::printf("usage: %s [--export [output-dir]]\n", argv[0]);
            return 0;
        } else if (a == "--export") {
            exportMode = true;
        } else if (!a.empty() && a[0] != '-') {
            out = a;
            exportMode = true;
        } else {
            std::printf("unknown option %s (try --help)\n", a.c_str());
            return 1;
        }
    }

    std::filesystem::create_directories(out);
    if (exportMode) {
        exportAll(out);
        return 0;
    }
    return runLiveViewer(out);
}
