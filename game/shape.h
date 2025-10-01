#pragma once

#include "arena.h"
#include <almond.h>

namespace Shape {

MeshData create_capsule(float radius, float height, int radial_segments, int rings, Arena* arena);

}
