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
static struct {
    intptr_t shift;
    intptr_t occupy; // 一个数据块包含的obj数目
} slub_objs_shift[] = {
    // ascending order
    {3, 0},
    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {9, 0},
    {10, 0},
    {11, 0},
};
#define OBJ_TYPE_COUNT          ARRAY_COUNT(slub_objs_shift)
#define MAX_OBJS_PER_BLOCK      (BLOCK_SIZE / (1 << slub_objs_shift[0].shift))
#define BYTES_OF_MAX_OBJS       ((MAX_OBJS_PER_BLOCK + 7) / 8)
#define RESOLV_OCCUPY()           \
        for (intptr_t i = 0; i < OBJ_TYPE_COUNT; ++i) {\
            intptr_t obj_size = (1 << slub_objs_shift[i].shift);\
            slub_objs_shift[i].occupy = (BLOCK_SIZE / obj_size + 7) / 8;\
            (void)fprintf(stderr, "%d\n", slub_objs_shift[i].occupy);\
        }


typedef struct {
    intptr_t blocks;
    char *bitmap;
    intptr_t bitmap_size;
    char *usemap;
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
    int rslt;
    slub_t *slb;
    intptr_t part_bits;
    char *bitmap, *usemap;

    assert(NULL != p);
    assert(size > 0);

    if ((size - BLOCK_SIZE) < BLOCK_SIZE) {
        (void)fprintf(stderr, "[ERROR] NO ENOUGH MEM\n");
        goto ERROR;
    }

    RESOLV_OCCUPY();

    slb = (slub_t *)p;
    slb->blocks = (size - BLOCK_SIZE) / BLOCK_SIZE;
    slb->bitmap = (char *)&slb[1];
    slb->bitmap_size = (slb->blocks + 7) / 8;
    assert(slb->bitmap_size > 0);
    slb->usemap = slb->bitmap + slb->bitmap_size * (OBJ_TYPE_COUNT + 1);
    slb->usemap_size = BYTES_OF_MAX_OBJS * slb->blocks;
    assert(slb->usemap_size > 0);

    // 初始化块归属位图
    // 第一个bitmap用于所有块使用情况
    bitmap = slb->bitmap;
    (void)memset(bitmap, 0, slb->bitmap_size * (OBJ_TYPE_COUNT + 1));
    part_bits = slb->blocks % 8;
    for (int i = 0; i < OBJ_TYPE_COUNT + 1; ++i) {
        bitmap += slb->bitmap_size;
        bitmap[-1] = ((~0) << part_bits);
    }

    // 初始化块使用位图
    usemap = slb->usemap;
    (void)memset(usemap, ~0, slb->usemap_size * OBJ_TYPE_COUNT);
    for (intptr_t i = 0; i < OBJ_TYPE_COUNT; ++i) {
        part_bits = BLOCK_SIZE / (1 << slub_objs_shift[i].shift);
        for (intptr_t j = 0; j < slb->blocks; ++j) {
            uint8_t *elmt = (uint8_t *)(usemap + slb->usemap_size * i);

            mem_shift_left(elmt + BYTES_OF_MAX_OBJS *j,
                           BYTES_OF_MAX_OBJS, part_bits);
        }
    }

    do {
        break;
ERROR:
        rslt = -1;
    } while (0);

    return rslt;
}

void dump_mem(void *p, intptr_t size)
{
    char *seg16;
    intptr_t segsize;

    seg16 = (char *)p;
    segsize = size;

    while (segsize > 0) {
        intptr_t max = (segsize > 16) ? 16 : segsize;

        fprintf(stderr, "%p:", seg16);
        for (intptr_t i = 0; i < max; ++i) {
            (void)fprintf(stderr, " %02hhx", seg16[i]);
        }
        (void)fprintf(stderr, "\n");
        seg16 += 16;
        segsize -= 16;
    }

    return;
}

void dump_slub(void *p)
{
    slub_t *slb;

    assert(NULL != p);

    slb = (slub_t *)p;
    (void)fprintf(stderr, "[DEBUG] slub: %p\n", slb);
    (void)fprintf(stderr, "[DEBUG] slub->blocks: %d\n", slb->blocks);
    (void)fprintf(stderr, "[DEBUG] slub->bitmap: %p\n", slb->bitmap);
    (void)fprintf(stderr, "[DEBUG] slub->bitmap_size: %d\n", slb->bitmap_size);
    (void)fprintf(stderr, "[DEBUG] slub->usemap: %p\n", slb->usemap);
    (void)fprintf(stderr, "[DEBUG] slub->usemap_size: %d\n", slb->usemap_size);

    (void)fprintf(stderr, "[DEBUG] slub->bitmap context:\n");
    dump_mem(slb->bitmap, slb->bitmap_size * (OBJ_TYPE_COUNT + 1));
    (void)fprintf(stderr, "[DEBUG] slub->usemap context:\n");
    dump_mem(slb->usemap, slb->usemap_size * OBJ_TYPE_COUNT);

    return;
}
