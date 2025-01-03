#pragma once
#include <stdint.h>
#include "vec.h"

struct incobj_face_t {
    int16_t start;      // pos in index buffer
    int8_t  length;
    int8_t  mat;        // material index
    vec3f   fn;         // precalculated face normal
};

struct incobj_idx_t {
    int16_t p, n, t, pad;   // additional element for padding
};

struct incobj_material_t {
    vec3f ka, ks, kd;       // ambient, diffuse and specular colors
    float Ns;               // specular power for phong shading

    // TODO: texture handles? since we are including textures in the file lol
};

// very simple mesh format, simple as FUCK :D
struct incobj_t {
    int16_t    total_pos;
    int16_t    total_normals;
    int16_t    total_texcoords;
    int16_t    total_indices;
    int16_t    total_faces;
    int16_t    total_materials; // should be dword aligned for now

    const vec3f             *p;   // vertex positions
    const vec3f             *n;   // vertex normals
    const vec2f             *t;   // vertex texture coordinates
    const incobj_idx_t      *i;   // index buffer
    const incobj_face_t     *f;   // faces
    const incobj_material_t *m; // materials
};

