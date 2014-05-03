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


// 最大内存块数
#define BLOCKS_MAX      32
#define BLOCK_SIZE      4096


// object type configuration, size = (1 << slub_objs_shift[i])
#define NONE_OBJ_SHIFT {0, 0}
static struct {
    intptr_t shift;
    intptr_t occupy; // 一个数据块能包含的obj数目
} slub_objs_shift[] = {
    // 必须升序
    {3, 0},
    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {9, 0},
    {10, 0},
    {11, 0},
    NONE_OBJ_SHIFT, // 岗哨
};
static intptr_t obj_type_count = 0;

#define MAX_OBJS_PER_BLOCK      (BLOCK_SIZE / (1 << slub_objs_shift[0].shift))
#define BYTES_OF_MAX_OBJS       ((MAX_OBJS_PER_BLOCK + 7) / 8)

// 宏RESOLV_OCCUPY用于初始化slub_objs_shift数组
#define RESOLV_OCCUPY()           \
        do {\
            obj_type_count = 0;\
            for (intptr_t i = 0; 0 != slub_objs_shift[i].shift; ++i) {\
                intptr_t obj_size = (1 << slub_objs_shift[i].shift);\
                slub_objs_shift[i].occupy = BLOCK_SIZE / obj_size;\
                ++obj_type_count;\
            }\
        } while (0)


typedef struct {
    intptr_t nblocks; // 内存块数目

    // 块归属位图，第一块记录全部块的使用情况，即是否被使用
    // 剩下的记录某种类型的数据使用了哪些块
    char *bitmap;

    // 单张位图大小，实际为(1 + obj_type_count)张
    intptr_t bitmap_size;

    // 块使用位图
    char *usemap;
    intptr_t usemap_size; // 单张位图大小

    void *self;
} slub_t;


static void __dump_mem__(void *p, intptr_t size);


// 内存左移
void mem_shift_left(void *p, intptr_t len, intptr_t n)
{
    uint8_t *base = (uint8_t *)(p);
    intptr_t empty_bytes = n / 8;
    intptr_t part_bits = n % 8;

    assert(NULL != p);
    assert(len > 0);
    assert(n > 0);

    // 移动位数过多被认为是清零
    if (n >= (len * 8)) {
        (void)memset(base, 0, len);
        goto EXIT;
    }

    if (empty_bytes > 0) {
        for (intptr_t i = len - empty_bytes - 1; i >= 0; --i) {
            base[i + empty_bytes] = base[i];
        }
        (void)memset(base, 0, empty_bytes);
    }

    for (intptr_t i = len - 1; i > empty_bytes; --i) {
        base[i] <<= part_bits;
        base[i] |= (base[i - 1] >> (8 - part_bits));
    }
    base[empty_bytes] <<= part_bits;

EXIT:
    return;
}

intptr_t mem_is_filled(void const *p, intptr_t len)
{
    char const *tmpbyte = (char const *)p;
    intptr_t const *tmpint = (intptr_t const *)p;
    intptr_t nints = len / sizeof(intptr_t);
    intptr_t nbytes = len % sizeof(intptr_t);

    assert(NULL != p);
    assert(len > 0);

    for (intptr_t i = 0; i < nints; ++i) {
        if ((~0) != tmpint[i]) {
            return FALSE;
        }
        tmpbyte = (char const *)&tmpint[i + 1];
    }

    for (intptr_t i = 0; i < nbytes; ++i) {
        if ((~0) != tmpbyte[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

intptr_t slub_format(void *p, intptr_t size)
{
    int rslt;
    slub_t *slb;
    intptr_t part_bits;
    intptr_t nblocks;
    char *bitmap, *usemap;

    assert(NULL != p);
    assert(size > 0);

    if ((size - BLOCK_SIZE) < BLOCK_SIZE) {
        (void)fprintf(stderr, "[ERROR] NO ENOUGH MEM\n");
        goto ERROR;
    }

    nblocks = size / BLOCK_SIZE;
    if (nblocks >= BLOCKS_MAX) {
        (void)fprintf(stderr, "[WARNING] TOO LARGE MEM\n");
        nblocks = BLOCKS_MAX - 1;
    }

    RESOLV_OCCUPY();

    slb = (slub_t *)p;
    slb->nblocks = nblocks;
    slb->bitmap = (char *)&slb[1];
    slb->bitmap_size = (slb->nblocks + 7) / 8;
    assert(slb->bitmap_size > 0);
    slb->usemap = slb->bitmap + slb->bitmap_size * (obj_type_count + 1);
    slb->usemap_size = 0;
    for (intptr_t i = 0; i < obj_type_count; ++i) {
        slb->usemap_size += (slub_objs_shift[i].occupy + 7) / 8;
    }
    assert(slb->usemap_size > 0);
    slb->self = p;

    // 初始化块归属位图
    bitmap = slb->bitmap;
    (void)memset(bitmap, 0, slb->bitmap_size * (obj_type_count + 1));
    part_bits = slb->nblocks % 8;
    for (int i = 0; i < obj_type_count + 1; ++i) {
        bitmap += slb->bitmap_size;
        bitmap[-1] = ((~0) << part_bits);
    }

    // 初始化块使用位图
    usemap = slb->usemap;
    (void)memset(usemap, ~0, slb->usemap_size * slb->nblocks);
    for (intptr_t i = 0; i < slb->nblocks; ++i) {
        intptr_t offset = 0;

        for (intptr_t j = 0; j < obj_type_count; ++j) {
            void *base;
            intptr_t occupy_len = ((slub_objs_shift[j].occupy + 7) / 8);

            base = usemap + slb->usemap_size * i + offset;
            mem_shift_left(base, occupy_len, slub_objs_shift[j].occupy);
            offset += occupy_len;
        }
    }

    do {
        break;
ERROR:
        rslt = -1;
    } while (0);

    return rslt;
}

void *slub_alloc(void *p, intptr_t obj_size)
{
    slub_t *slb = (slub_t *)p;

    assert(NULL != p);
    assert(obj_size > 0);



    return NULL;
}

void slub_free(void *p, void *obj, intptr_t obj_size)
{
}

void __dump_mem__(void *p, intptr_t size)
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
    (void)fprintf(stderr, "[DEBUG] slub->blocks: %d\n", slb->nblocks);
    (void)fprintf(stderr, "[DEBUG] slub->bitmap: %p\n", slb->bitmap);
    (void)fprintf(stderr, "[DEBUG] slub->bitmap_size: %d\n", slb->bitmap_size);
    (void)fprintf(stderr, "[DEBUG] slub->usemap: %p\n", slb->usemap);
    (void)fprintf(stderr, "[DEBUG] slub->usemap_size: %d\n", slb->usemap_size);
    (void)fprintf(stderr, "[DEBUG] slub->self: %p\n", slb->self);

    (void)fprintf(stderr, "[DEBUG] slub->bitmap context:\n");
    assert(obj_type_count > 0);
    __dump_mem__(slb->bitmap, slb->bitmap_size * (obj_type_count + 1));
    (void)fprintf(stderr, "[DEBUG] slub->usemap context:\n");
    __dump_mem__(slb->usemap, slb->usemap_size * slb->nblocks);

    return;
}
