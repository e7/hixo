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


#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "hixo.h"
#include "list.h"

typedef struct {
    int x;
    list_t m_node;
} integer_t;

int main(int argc, char *argv[])
{
#define DATA_COUNT      4
    list_t *p_node = NULL;
    hixo_resource_t int_rsc = {};

    if (HIXO_ERROR == create_resource(&int_rsc,
                                      DATA_COUNT,
                                      sizeof(integer_t),
                                      OFFSET_OF(integer_t, m_node)))
    {
        return EXIT_FAILURE;
    }
    for (int i = 0; i < DATA_COUNT; ++i) {
        integer_t *p_integer = alloc_resource(&int_rsc);
        
        p_integer->x = i;
    }
    assert(NULL == int_rsc.mp_free_list);
    p_node = int_rsc.mp_inuse_list;
    for ( ; ; ) {
        integer_t *p_integer = NULL;

        if (NULL == p_node) {
            break;
        }

        p_integer = CONTAINER_OF(p_node, integer_t, m_node);
        fprintf(stderr, "%d\n", p_integer->x);
        p_node = *(list_t **)p_node;
        free_resource(&int_rsc, p_integer);
    }
    assert(NULL == int_rsc.mp_inuse_list);
    p_node = int_rsc.mp_free_list;
    for ( ; ; ) {
        integer_t *p_integer = NULL;

        if (NULL == p_node) {
            break;
        }

        p_integer = CONTAINER_OF(p_node, integer_t, m_node);
        fprintf(stderr, "%d\n", p_integer->x);
        p_node = *(list_t **)p_node;
    }
    destroy_resource(&int_rsc);
                                      
    return EXIT_SUCCESS;
}
