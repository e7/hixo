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
#endif // __RESOURCE_H__
