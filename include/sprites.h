// sprites.h - sprite sheets generated at start-up with the project's own
// drawing algorithms (no image files are loaded).
#pragma once
#include "raster.h"

// Colour-key used as the "transparent" background of every sprite sheet.
constexpr Color SPRITE_KEY{255, 0, 255};

// 8-frame walk cycle of the shadow figure, 48x96 pixels per frame, facing right.
// Built once and cached.
const SpriteSheet& shadowWalkerSheet();
