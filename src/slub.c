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
    intptr_t magic_no; // 0xE78F8A

    intptr_t nblocks; // 内存块数目

    // 块归属位图，第一块记录全部块的使用情况，即是否被使用
    // 剩下的记录某种类型的数据使用了哪些块
    char *bitmap;

    // 单张位图大小，实际为(1 + obj_type_count)张
    intptr_t bitmap_size;

    // 块使用位图
    char *usemap;
    intptr_t usemap_size; // 单张位图大小

    // 内存块
    char *block;

    void *self;
} slub_t;


static void dump_mem(void *p, intptr_t size);


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
        tmpbyte = (char const *)&tmpint[i + 1];

        if ((~0) != tmpint[i]) {
            return FALSE;
        }
    }

    for (intptr_t i = 0; i < nbytes; ++i) {
        if ((~0) != tmpbyte[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

void mem_set_bit(void *p, intptr_t len, intptr_t n, intptr_t value)
{
    char *tmp = (char *)p;
    intptr_t nbytes = n / 8;
    intptr_t part_bits = n % 8;

    assert(NULL != p);
    assert(n >= 0);
    assert(n < len * 8);

    if (value) {
        tmp[nbytes] |= (1 << part_bits);
    } else {
        tmp[nbytes] &= ~(1 << part_bits);
    }
}

// 内存位探测
intptr_t mem_detect_bit(void const *p, intptr_t len, intptr_t zero)
{
    char const *tmpbyte = (char const *)p;

    assert(NULL != p);
    assert(len > 0);

    for (intptr_t i = 0; i < len; ++i) { // 一定能进循环
        if (zero) { // 0位探测
            if ((~0) == tmpbyte[i]) {
                continue;
            }
            for (intptr_t j = 0; j < 8; ++j) {
                if (tmpbyte[i] & (1 << j)) {
                    continue;
                }
                return i * 8 + j;
            }
        } else { // 1位探测
            if (0 == tmpbyte[i]) {
                continue;
            }
            for (intptr_t j = 0; j < 8; ++j) {
                if (tmpbyte[i] & (1 << j)) {
                    return i * 8 + j;
                }
            }
        }
    }

    return -1;
}

intptr_t slub_format(void *p, intptr_t size)
{
    int rslt;
    slub_t *slb;
    intptr_t nblocks;
    intptr_t part_bits;
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
    slb->magic_no = 0xE78F8A;
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
    slb->block = ((char *)p + BLOCK_SIZE);
    slb->self = p;

    // 初始化块归属位图
    bitmap = slb->bitmap;
    part_bits = slb->nblocks % 8;
    (void)memset(bitmap, 0, slb->bitmap_size * (obj_type_count + 1));
    slb->bitmap[slb->bitmap_size - 1] = ((~0) << part_bits); // top bitmap

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

static intptr_t
alloc_index_from_top_bitmap(slub_t *slb, intptr_t type)
{
    intptr_t rslt;
    char *top_bitmap;
    char *type_bitmap;

    rslt = -1;
    top_bitmap = slb->bitmap;
    type_bitmap = &slb->bitmap[slb->bitmap_size * (type + 1)];
    for (intptr_t i = 0; i < slb->bitmap_size; ++i) {
        if ((~0) != top_bitmap[i]) {
            for (intptr_t j = 0; j < 8; ++j) {
                if (0 == (top_bitmap[i] & (1 << j))) {
                    top_bitmap[i] |= (1 << j);
                    rslt = (i * 8 + j);
                    mem_set_bit(type_bitmap, slb->bitmap_size, rslt, 1);
                    break;
                }
            }
            break;
        }
    }

    return rslt;
}

static intptr_t
alloc_index_from_usemap(intptr_t type, char *usemap, intptr_t usemap_size)
{
    intptr_t rslt;
    intptr_t offset; // 在usemap中的起始位置偏移

    // usemap中属于该类型的字节数
    intptr_t content_size = (slub_objs_shift[type].occupy + 7) / 8;

    rslt = -1;
    offset = 0;
    for (intptr_t i = 0; i < type; ++i) {
        offset += (slub_objs_shift[i].occupy + 7) / 8;
    }

    for (intptr_t i = 0; i < content_size; ++i) {
        if ((~0) != usemap[offset + i]) {
            for (intptr_t j = 0; j < 8; ++j) {
                if (0 == (usemap[offset + i] & (1 << j))) {
                    rslt = i * 8 + j;
                    usemap[offset + i] |= (1 << j); // 置位表示已使用
                    break;
                }
            }
            break;
        }
    }

    return rslt;
}

static void *slub_alloc_from_existed(slub_t *slb,
                                     intptr_t type)
{
    void *rslt;
    char *usemap;
    char *type_bitmap;
    intptr_t type_size;

    rslt = NULL;
    type_size = (1 << slub_objs_shift[type].shift);
    type_bitmap = &slb->bitmap[slb->bitmap_size * (type + 1)];
    for (intptr_t i = 0; i < slb->bitmap_size; ++i) {
        if (0 != type_bitmap[i]) {
            char *block_ptr;
            intptr_t rslt_index;

            for (intptr_t j = 0; j < 8; ++j) {
                if (0 == (type_bitmap[i] & (1 << j))) {
                    continue;
                }

                // (i * 8 + j)即为usemap索引
                usemap = &slb->usemap[(i * 8 + j) * (slb->usemap_size)];
                rslt_index = alloc_index_from_usemap(type,
                                                     usemap,
                                                     slb->usemap_size);
                if (-1 != rslt_index) {
                    block_ptr = &slb->block[(i * 8 + j) * BLOCK_SIZE];
                    rslt = &block_ptr[rslt_index * type_size];
                    break;
                }
            }

            break;
        }
    }

    return rslt;
}

void *slub_alloc(void *p, intptr_t obj_size)
{
    void *rslt;
    intptr_t type; // 对象类型索引
    intptr_t block_index;
    slub_t *slb = (slub_t *)p;

    assert(NULL != p);
    assert(obj_size > 0);

    assert(0xE78F8A == slb->magic_no);

    // 寻找合适的对象类型
    type = -1;
    for (intptr_t i = 0; 0 != slub_objs_shift[i].shift; ++i) {
        if (obj_size <= (1 << slub_objs_shift[i].shift)) {
            type = i;
            break;
        }
    }
    assert(-1 != type);

    // 先查询旗下归属块
    rslt = slub_alloc_from_existed(slb, type);
    if (NULL != rslt) {
        goto EXIT;
    }

    // 需要新块
    block_index = alloc_index_from_top_bitmap(slb, type);
    if (-1 == block_index) {
        rslt = NULL;
        goto EXIT;
    }
    rslt = slub_alloc_from_existed(slb, type);
    assert(NULL != rslt);

EXIT:
    return rslt;
}

void slub_free(void *p, void *obj, intptr_t obj_size)
{
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
    for (intptr_t i = 0; 0 != slub_objs_shift[i].shift; ++i) {
        (void)fprintf(stderr,
                      "[DEBUG] slub_objs_shift[i]: (%d, %d)\n",
                      slub_objs_shift[i].shift,
                      slub_objs_shift[i].occupy);
    }
    (void)fprintf(stderr, "[DEBUG] slub: %p\n", slb);
    (void)fprintf(stderr, "[DEBUG] slub->nblocks: %d\n", slb->nblocks);
    (void)fprintf(stderr, "[DEBUG] slub->bitmap: %p\n", slb->bitmap);
    (void)fprintf(stderr, "[DEBUG] slub->bitmap_size: %d\n", slb->bitmap_size);
    (void)fprintf(stderr, "[DEBUG] slub->usemap: %p\n", slb->usemap);
    (void)fprintf(stderr, "[DEBUG] slub->usemap_size: %d\n", slb->usemap_size);
    (void)fprintf(stderr, "[DEBUG] slub->block: %p\n", slb->block);
    (void)fprintf(stderr, "[DEBUG] slub->self: %p\n", slb->self);

    (void)fprintf(stderr, "[DEBUG] slub->bitmap context:\n");
    assert(obj_type_count > 0);
    dump_mem(slb->bitmap, slb->bitmap_size * (obj_type_count + 1));
    (void)fprintf(stderr, "[DEBUG] slub->usemap context:\n");
    dump_mem(slb->usemap, slb->usemap_size * slb->nblocks);

    for (intptr_t i = 0; i < slb->nblocks; ++i) {
        (void)fprintf(stderr, "[DEBUG] slub->block[%d] context:\n", i);
        dump_mem(&slb->block[i * BLOCK_SIZE], BLOCK_SIZE);
    }

    return;
}
