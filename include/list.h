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


#ifndef __LIST_H__
#define __LIST_H__

#include "common.h"

#if __cplusplus
extern "C" {
#endif // __cplusplus

// 单链表
typedef intptr_t list_t;

static inline
void add_node(list_t **pp_list, list_t *p_node)
{
    list_t *p_tmp = NULL;

    p_tmp = *pp_list;
    *p_node = (list_t)p_tmp;
    *pp_list = p_node;

    return;
}

static inline
int rm_node(list_t **pp_list, list_t *p_node)
{
    int removed = FALSE;
    list_t **pp_curr = NULL;

    pp_curr = pp_list;
    while (*pp_curr) {
        if (p_node == *pp_curr) {
            *pp_curr = (list_t *)*p_node;
            *p_node = 0;
            removed = TRUE;

            break;
        }

        pp_curr = (list_t **)*pp_curr;
    }

    return removed;
}
#if __cplusplus
}
#endif // __cplusplus
#endif // __LIST_H__
