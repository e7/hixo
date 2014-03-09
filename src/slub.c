// Copyright [2013] [E7, ryuuzaki.uchiha@gmail.com]

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "slub.h"


#define BLOCK_SIZE      4096


// object type configuration, size = (1 << slub_objs_shift[i])
static intptr_t slub_objs_shift[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, // ascending order
};
#define OBJ_TYPE_COUNT          ARRAY_COUNT(slub_objs_shift)
#define MAX_OBJS_PER_BLOCK      (BLOCK_SIZE / (1 << slub_objs_shift[0]))


typedef struct {
    intptr_t blocks;
    intptr_t bitmap_size;
    intptr_t usemap_size;
} slub_t;

// 内存左移
void mem_shift_left(uint8_t *p, intptr_t len, intptr_t n)
{
    intptr_t empty_bytes = n / 8;
    intptr_t part_bits = n % 8;

    assert(NULL != p);
    assert(len > 0);
    assert(n > 0);

    if (n >= (len * 8)) {
        (void)memset(p, 0, len);
        goto EXIT;
    }

    if (empty_bytes > 0) {
        for (intptr_t i = len - empty_bytes - 1; i >= 0; --i) {
            p[i + empty_bytes] = p[i];
        }
        (void)memset(p, 0, empty_bytes);
    }

    for (intptr_t i = len - 1; i > empty_bytes; --i) {
        p[i] <<= part_bits;
        p[i] |= (p[i - 1] >> (8 - part_bits));
    }
    p[empty_bytes] <<= part_bits;

EXIT:
    return;
}

int make_slub(void *p, intptr_t size)
{
    int rslt = 0;
    slub_t *slb = (slub_t *)p;
    intptr_t part_bits = 0;
    char *byte = (char *)p;

    assert(NULL != p);
    assert(size > 0);

    if ((size - BLOCK_SIZE) < (BLOCK_SIZE * OBJ_TYPE_COUNT)) {
        (void)fprintf(stderr, "[ERROR] NO ENOUGH MEM\n");
        goto ERROR;
    }

    slb->blocks = (size - BLOCK_SIZE) / BLOCK_SIZE;

    slb->bitmap_size = (slb->blocks + 8) / 8;
    assert(slb->bitmap_size > 0);
    part_bits = slb->blocks % 8;
    for (int i = 0; i < OBJ_TYPE_COUNT; ++i) {
        byte += slb->bitmap_size;
        byte[-1] = ((~0) << part_bits);
    }

    slb->usemap_size = (MAX_OBJS_PER_BLOCK + 8) / 8;
    assert(slb->usemap_size > 0);
    for (int i = 0; i < OBJ_TYPE_COUNT; ++i) {
        part_bits = BLOCK_SIZE / (1 << slub_objs_shift[i]);
    }

    do {
        break;
ERROR:
        rslt = -1;
    } while (0);

    return rslt;
}
