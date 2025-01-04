#pragma once 
#include <stdint.h>
#include <incobj.h>

#ifdef __cplusplus
extern "C" {
#endif

struct face_sort_t {
    uint32_t depth;
    const incobj_face_t *face;
};

// sort an array of face_sort_t
void face_sort(face_sort_t *p, uint32_t count);

// radix8 sort using a temporary array of same size
void face_sort_radix(face_sort_t *p, face_sort_t *p_tmp, uint32_t count);

#ifdef __cplusplus
}
#endif
