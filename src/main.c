#include "map.h"
#include "geometry.h"

Brush create_quake_brush(Arena* arena) {
    Brush brush = {0};
    brush.points = PushArrayAligned(arena, Plane, 6);
    brush.count = 6;

    // Plane 1: ( 224 272 304 ) ( 224 624 304 ) ( 224 272 336 )
    // This is a vertical plane at x = 224
    vec3 p1_a = {224, 272, 304};
    vec3 p1_b = {224, 624, 304};
    vec3 p1_c = {224, 272, 336};
    brush.points[0] = plane_from_points(p1_a, p1_b, p1_c);

    // Plane 2: ( 240 544 304 ) ( -384 544 304 ) ( 240 544 336 )
    // This is a vertical plane at y = 544
    vec3 p2_a = {240, 544, 304};
    vec3 p2_b = {-384, 544, 304};
    vec3 p2_c = {240, 544, 336};
    brush.points[1] = plane_from_points(p2_a, p2_b, p2_c);

    // Plane 3: ( 240 272 384 ) ( 240 624 384 ) ( -384 272 384 )
    // This is a horizontal plane at z = 384
    vec3 p3_a = {240, 272, 384};
    vec3 p3_b = {240, 624, 384};
    vec3 p3_c = {-384, 272, 384};
    brush.points[2] = plane_from_points(p3_a, p3_b, p3_c);

    // Plane 4: ( -384 272 416 ) ( -384 624 416 ) ( 240 272 416 )
    // This is a horizontal plane at z = 416
    vec3 p4_a = {-384, 272, 416};
    vec3 p4_b = {-384, 624, 416};
    vec3 p4_c = {240, 272, 416};
    brush.points[3] = plane_from_points(p4_a, p4_b, p4_c);

    // Plane 5: ( 240 704 336 ) ( -384 704 336 ) ( 240 704 304 )
    // This is a vertical plane at y = 704
    vec3 p5_a = {240, 704, 336};
    vec3 p5_b = {-384, 704, 336};
    vec3 p5_c = {240, 704, 304};
    brush.points[4] = plane_from_points(p5_a, p5_b, p5_c);

    // Plane 6: ( 384 272 336 ) ( 384 624 336 ) ( 384 272 304 )
    // This is a vertical plane at x = 384
    vec3 p6_a = {384, 272, 336};
    vec3 p6_b = {384, 624, 336};
    vec3 p6_c = {384, 272, 304};
    brush.points[5] = plane_from_points(p6_a, p6_b, p6_c);

    return brush;
}

int main() {
    Arena arena;
    arena_init(&arena);

    Brush test = create_quake_brush(&arena);

    brush_to_mesh(test, &arena);

    // load_map("./content/celeste.map");
}