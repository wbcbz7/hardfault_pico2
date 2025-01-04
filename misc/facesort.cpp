#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include "facesort.h"

static int facesort_comp_qsort(const void *a, const void *b) {
    return ((const face_sort_t*)a)->depth - ((const face_sort_t*)b)->depth;
}

bool facesort_comp_stdsort(const face_sort_t &lhs, const face_sort_t &rhs)
{
    return lhs.depth < rhs.depth;
}

#if 0
void face_sort(face_sort_t *p, uint32_t size) {
    qsort(p, count, sizeof (struct face_sort_t), facesort_comp_qsort);
}
#else
void face_sort(face_sort_t *p, uint32_t count) {
    std::sort(p, p + count, facesort_comp_stdsort);
}
#endif

void face_sort_radix(face_sort_t *p, face_sort_t *p_tmp, uint32_t count) {
    if ((count <= 0) || (p == NULL) || (p_tmp == NULL)) return;

    // histogram for each pass and possible byte value
    static int32_t histogram[4][256];
    static face_sort_t *indices[256];

    // clear histogram
    memset(histogram, 0, sizeof(histogram));
    bool isAlreadySorted = true;

    // fill histograms
    int32_t c = count;

    // check for already sorted array
    face_sort_t *key = p;
    uint32_t previousValue = key->depth;
    do {
        if (previousValue < key->depth) {isAlreadySorted = false; break;}
        previousValue = key->depth;
        histogram[0][*((uint8_t*)key + 0)]++;
        histogram[1][*((uint8_t*)key + 1)]++;
        histogram[2][*((uint8_t*)key + 2)]++;
        histogram[3][*((uint8_t*)key + 3)]++;
        key++; 
    } while (--c);

    // array is already sorted - nothing to do!
    if (isAlreadySorted) return;

    // calculate histograms for rest of array
    do {
        histogram[0][*((uint8_t*)key + 0)]++;
        histogram[1][*((uint8_t*)key + 1)]++;
        histogram[2][*((uint8_t*)key + 2)]++;
        histogram[3][*((uint8_t*)key + 3)]++;
        key++; 
    } while (--c);

    // perform sorting passes
    face_sort_t* src = p; face_sort_t* dst = p_tmp; face_sort_t *tmp;
    face_sort_t *s, *d;
    for (int b = 0; b < 4; b++) {       // b is also used as byte offset
        // merge histograms
        indices[0] = dst;
        for (int i = 1; i < 256; i++) {
            indices[i] = indices[i-1] + histogram[b][i-1];
        }

        // sort
        c = count;
        s = src;
        do {
            *indices[*((uint8_t*)s + b)]++ = *s; s++;
        } while (--c);

        // swap src and dst
        tmp = src;
        src = dst;
        dst = tmp;
    }

    return;
}

