//
// Created by ivang on 6/10/2026.
//

// scene.h - the Shattered Realities scene, drawn only with the project's own algorithms
#pragma once
#include "framebuffer.h"

// distortion: 0.0 = clean, healthy reality ... 1.0 = fully shattered reality
void renderScene(Framebuffer& fb, double distortion);