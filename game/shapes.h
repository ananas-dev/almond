#pragma once

#include <almond.h>
#include "arena.h"

MeshData make_capsule(float radius, float height, int radial_segments, int rings, Arena* arena);

