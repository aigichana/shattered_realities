// main.cpp - renders every checkpoint image into <project>/output
#include "demos.h"
#include "framebuffer.h"
#include "scene.h"
#include <cstdio>
#include <filesystem>
#include <string>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : std::string(PROJECT_ROOT) + "/output";
    std::filesystem::create_directories(out);

    std::printf("Rendering technique demos...\n");
    demoLines(out);
    demoCirclesEllipses(out);
    demoPolygonsFill(out);
    demoClipping(out);
    demoCurves(out);

    std::printf("Rendering Shattered Realities scene stages...\n");
    const double stages[] = {0.0, 0.35, 0.7, 1.0};
    const char* names[] = {"06_scene_stage1_healthy", "07_scene_stage2_unease",
                           "08_scene_stage3_distorting", "09_scene_stage4_shattered"};
    for (int i = 0; i < 4; ++i) {
        Framebuffer fb(640, 480);
        renderScene(fb, stages[i]);
        fb.saveBMP(out + "/" + names[i] + ".bmp");
        std::printf("  wrote %s/%s.bmp (distortion %.2f)\n", out.c_str(), names[i], stages[i]);
    }
    std::printf("Done.\n");
    return 0;
}