// scene.h - the Shattered Realities scene, drawn only with the project's own algorithms
#pragma once
#include "framebuffer.h"

struct SceneOptions {
    bool construction = false;  // overlay control polygons, clip windows and pivots (XOR-blitted)
    bool captions = true;       // story captions, title and end card
};

// Draws the story at one moment (seconds, see story.h). The picture is a pure
// function of the time, so any moment can be paused, scrubbed or exported.
void renderStory(Framebuffer& fb, double storyTime, const SceneOptions& opt = {});
