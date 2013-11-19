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


#ifndef __RESOURCE_H__
#define __RESOURCE_H__


#include "list.h"


typedef struct {
    void *mp_start;
    void *mp_end;
    int m_offset; // offset of list node
    list_t *mp_free_list;
    int m_used;
    int m_capacity;
} hixo_resource_t;


extern int create_resource(hixo_resource_t *p_rsc,
                           size_t count,
                           size_t elemt_size,
                           size_t offset);
extern void *alloc_resource(hixo_resource_t *p_rsc);
extern void free_resource(hixo_resource_t *p_rsc, void *p_elemt);
extern void destroy_resource(hixo_resource_t *p_rsc);


/////////////////////////////////////////////////////////////////////
#define __VFTS__ __vfts__
#define DECLARE_VFTS void **__VFTS__
#define VFTS_OFFSET(objtype) OFFSET_OF(objtype, __VFTS__)

#define __REF_VFTS__(obj, vfts_offset) \
            (*(void ***)((uint8_t *)obj + vfts_offset))
#define SET_VFTS_VALUE(obj, vfts_offset, value) \
            __REF_VFTS__(obj, vfts_offset) = value
#define GET_INTERFACE(obj, vfts_offset, index, interface) \
            ((interface *)__REF_VFTS__(obj, vfts_offset)[index])


typedef struct {
    void *(*__new__)(void *pool, ssize_t element_size, ssize_t count);
    void (*__del__)(void *pool);
} hixo_pool_t;

static inline
void *hixo_call_pool_new(void *obj,
                         intptr_t offset,
                         intptr_t index,
                         ssize_t element_size,
                         ssize_t count)
{
    hixo_pool_t *ops = GET_INTERFACE(obj, offset, index, hixo_pool_t);

    return (*ops->__new__)(obj, element_size, count);
}

static inline
void hixo_call_pool_del(void *obj, intptr_t offset, intptr_t index)
{
    hixo_pool_t *ops = GET_INTERFACE(obj, offset, index, hixo_pool_t);

    (*ops->__del__)(obj);

    return;
}

// mempool
enum {
    ITFC_INDEX_MEMPOOL_POOL = 0,
};
typedef struct {
    void *__data__;
    DECLARE_VFTS;
} hixo_mempool_t;
extern int mempool_init(hixo_mempool_t *mempool);
extern void mempool_exit(hixo_mempool_t *mempool);
#endif // __RESOURCE_H__
