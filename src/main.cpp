// main.cpp - entry point.
//   shattered_realities                 open the live viewer (Windows); elsewhere export images
//   shattered_realities --export [dir]  write every technique page and scene stage as PNG
//   shattered_realities <dir>           same as --export <dir>
//   shattered_realities --frame <seconds> <file.png>   save one moment of the story
#include "app.h"
#include "demos.h"
#include "scene.h"
#include <chrono>
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
            std::printf("usage: %s [--export [output-dir]] [--frame seconds file.png] [--bench]\n", argv[0]);
            return 0;
        } else if (a == "--frame" && i + 2 < argc) {
            Framebuffer canvas(CANVAS_W, CANVAS_H);
            renderStory(canvas, std::stod(argv[i + 1]));
            return canvas.savePNG(argv[i + 2]) ? 0 : 1;
        } else if (a == "--bench") {  // render the whole story and report the time per frame
            Framebuffer canvas(CANVAS_W, CANVAS_H);
            double worst = 0, total = 0, worstAt = 0;
            int n = 0;
            for (double t = 0; t < 101; t += 0.1, ++n) {
                auto t0 = std::chrono::steady_clock::now();
                renderStory(canvas, t);
                double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                total += ms;
                if (ms > worst) { worst = ms; worstAt = t; }
            }
            std::printf("%d frames: average %.2f ms, slowest %.2f ms at %.1f s\n", n, total / n, worst, worstAt);
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
