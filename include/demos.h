// demos.h - one demonstration page per graphics technique (checkpoint evidence).
// Each page renders into a 640x480 canvas; the same pages are exported as PNG
// files and shown live (and animated) in the interactive viewer.
#pragma once
#include "framebuffer.h"
#include <string>
#include <vector>

constexpr int CANVAS_W = 640, CANVAS_H = 480;  // drawing area of a page / the scene
constexpr int CAPTION_H = 44;                  // title bar above it
constexpr int PAGE_W = CANVAS_W, PAGE_H = CANVAS_H + CAPTION_H;

struct DemoPage {
    const char* file;      // output file stem, e.g. "01_lines_dda_bresenham"
    const char* title;     // shown in the caption bar
    const char* subtitle;
    void (*render)(Framebuffer& canvas, double time);
    bool animated;         // re-rendered every frame in the live viewer
};
const std::vector<DemoPage>& demoPages();

// Draws the caption bar into rows [0, CAPTION_H) of a PAGE_W x PAGE_H framebuffer.
void drawCaptionBar(Framebuffer& page, const std::string& title, const std::string& subtitle,
                    const std::string& right);

// Caption bar + canvas copied below it with a bit-blit.
Framebuffer composePage(const Framebuffer& canvas, const std::string& title,
                        const std::string& subtitle, const std::string& right);

// Writes every demo page and the scene stages as PNG files, and prints the
// numerical algorithm checks.
void exportAll(const std::string& outDir);
