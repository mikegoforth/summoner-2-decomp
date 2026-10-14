// 3D primitives: camera-facing ribbons and wireframe spheres. The file has no data.
#include "game/unit_00182180.h"
#include "game/unit_00180B70.h"
#include "game/unit_0017CEB0.h"
#include "game/unit_001843A0.h"

extern "C" float func_001CC200(float x); // cosf
extern "C" float func_001CC4B8(float x); // sinf

int draw_ribbon(int count, const vector3* points, const color* colors, const u8* alphas, float width, gr_mode mode) {
    vector3 edges[42];
    vertex vertices[42];
    vector3 last_side;
    int i;

    // Each point gets two edge points, half the width to either side along the perpendicular to
    // the segment that starts there and to the direction from the segment's midpoint to the
    // camera. Inner points average the sides of the two segments they join so that neighbouring
    // quads share their edge.
    for (i = 0; i < count - 1; i++) {
        vector3 dir = points[i] - points[i + 1];
        dir.normalize_safe();
        vector3 mid = (points[i] + points[i + 1]) * 0.5f;
        vector3 to_camera = View_position - mid;
        to_camera.normalize_safe();
        vector3 side = dir.cross(to_camera);
        side.normalize_safe();
        vector3 prev_side = last_side;
        last_side = side;
        if (i != 0) {
            side = (prev_side + side) / 2.0f;
            side.normalize_safe();
        }
        edges[i * 2] = points[i] + side * (0.5f * width);
        edges[i * 2 + 1] = points[i] - side * (0.5f * width);
    }

    // The last point uses the last segment as it is.
    vector3 dir = points[count - 2] - points[count - 1];
    dir.normalize_safe();
    vector3 mid = (points[count - 2] + points[count - 1]) * 0.5f;
    vector3 to_camera = View_position - mid;
    to_camera.normalize_safe();
    vector3 side = dir.cross(to_camera);
    side.normalize_safe();
    edges[(count - 1) * 2] = points[count - 1] + side * (0.5f * width);
    edges[(count - 1) * 2 + 1] = points[count - 1] - side * (0.5f * width);
    rotate_vertices(vertices, edges, count * 2);

    // One quad per segment, given the view depth of the segment's midpoint as its sort depth.
    for (i = 0; i < count - 1; i++) {
        prim_begin(1, ((points[i] * 0.5f + points[i + 1] * 0.5f) - View_position).rotated(View_matrix).z, 0, 0.0f);
        prim_vertex(&vertices[i * 2]);
        prim_vertex(&vertices[i * 2 + 1]);
        prim_vertex(&vertices[(i + 1) * 2 + 1]);
        prim_vertex(&vertices[(i + 1) * 2]);
        prim_layer(mode, gr_screen.current_bitmap, gr_screen.current_color.r, gr_screen.current_color.g,
                   gr_screen.current_color.b, gr_screen.current_color.a, 0);
        if (colors) {
            prim_rgb(colors[i].r, colors[i].g, colors[i].b);
            prim_rgb(colors[i + 1].r, colors[i + 1].g, colors[i + 1].b);
            prim_rgb(colors[i + 1].r, colors[i + 1].g, colors[i + 1].b);
            prim_rgb(colors[i].r, colors[i].g, colors[i].b);
        }
        if (alphas) {
            prim_alpha(alphas[i]);
            prim_alpha(alphas[i + 1]);
            prim_alpha(alphas[i + 1]);
            prim_alpha(alphas[i]);
        }
        prim_uv(0.0f, 0.0f);
        prim_uv(1.0f, 0.0f);
        prim_uv(1.0f, 1.0f);
        prim_uv(0.0f, 1.0f);
        prim_end();
    }
    return 1;
}

int draw_wire_sphere(const vector3& center, float radius, gr_mode mode) {
    vertex first;
    vertex prev;
    vertex cur;
    vertex tmp;
    matrix3 orient;
    vector3 p;
    int segments;
    int axis;

    orient = Unscaled_matrix;
    rotate_vertex(&first, center);
    segments = (int)get_screen_size(&first, radius);
    clamp(segments, 5, 32);
    for (axis = 0; axis < 3; axis++) {
        switch (axis) {
        case 0:
            orient.set_from_unit_forward(vector3(1.0f, 0.0f, 0.0f));
            break;
        case 1:
            orient.set_from_unit_forward(vector3(0.0f, 1.0f, 0.0f));
            break;
        case 2:
            orient.set_from_unit_forward(vector3(0.0f, 0.0f, 1.0f));
            break;
        }

        // The circle in the plane of the right and up vectors, starting along the up vector.
        float step = 6.2831855f / (float)segments;
        float angle = 0.0f;
        float x = radius * func_001CC4B8(angle);
        p = center + orient.rvec * x + orient.uvec * (radius * func_001CC200(angle));
        rotate_vertex(&first, p);
        prev = first;
        for (int i = 0; i < segments; i++) {
            float x = radius * func_001CC4B8(angle);
            p = center + orient.rvec * x + orient.uvec * (radius * func_001CC200(angle));
            rotate_vertex(&cur, p);
            draw_line(&prev, &cur, mode);
            angle += step;
            tmp = prev;
            prev = cur;
            cur = tmp;
        }
        draw_line(&prev, &first, mode);

        // A line through the center along the circle's axis.
        p = center + orient.fvec * radius;
        rotate_vertex(&first, p);
        p = center - orient.fvec * radius;
        rotate_vertex(&prev, p);
        draw_line(&first, &prev, mode);
    }
    return 1;
}
