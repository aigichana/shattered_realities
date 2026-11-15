// scene.h - the Shattered Realities scene, drawn only with the project's own algorithms
#pragma once
#include "framebuffer.h"

struct SceneOptions {
    double time = 0;            // seconds; drives the idle animation (lamp swing, smoke, walker...)
    bool construction = false;  // overlay control polygons, clip windows and pivots (XOR-blitted)
};

// distortion: 0.0 = clean, healthy reality ... 1.0 = fully shattered reality
void renderScene(Framebuffer& fb, double distortion, const SceneOptions& opt = {});

// Human-readable stage name for a distortion value.
const char* stageName(double distortion);
