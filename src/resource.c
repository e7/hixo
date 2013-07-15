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


#include "resource.h"


int create_resource(hixo_resource_t *p_rsc,
                    size_t count,
                    size_t elemt_size,
                    size_t offset)
{
    list_t *p_node = NULL;

    if ((0 == count) || (0 == elemt_size)) {
        return HIXO_ERROR;
    }

    if (NULL != p_rsc->mp_start) {
        fprintf(stderr, "[ERROR] resource exists\n");

        return HIXO_ERROR;
    }

    // start
    p_rsc->mp_start = calloc(count, elemt_size);
    if (NULL == p_rsc->mp_start) {
        fprintf(stderr, "[ERROR} out of memory\n");

        return HIXO_ERROR;
    }
    p_rsc->mp_end = ((uint8_t *)p_rsc->mp_start) + count * elemt_size;
    p_rsc->m_offset = offset;
    p_rsc->mp_free_list = NULL;
    for (int i = 0; i < count; ++i) {
        p_node = (list_t *)(((uint8_t *)p_rsc->mp_start)
                                + i * elemt_size
                                + offset);
        add_node(&p_rsc->mp_free_list, p_node);
    }
    p_rsc->m_used = 0;
    p_rsc->m_capacity = count;

    return HIXO_OK;
}

void *alloc_resource(hixo_resource_t *p_rsc)
{
    list_t *p_node = NULL;

    if (NULL == p_rsc->mp_free_list) {
        return NULL;
    }
    p_node = p_rsc->mp_free_list;
    assert(rm_node(&p_rsc->mp_free_list, p_node));
    ++p_rsc->m_used;

    return ((uint8_t *)p_node) - p_rsc->m_offset;
}

void free_resource(hixo_resource_t *p_rsc, void *p_elemt)
{
    list_t *p_node = NULL;

    if (NULL == p_elemt) {
        goto ERR_INVALID_ELEMT;
    }
    if ((p_elemt < p_rsc->mp_start) || (p_elemt >= p_rsc->mp_end)) {
        goto ERR_INVALID_ELEMT;
    }

    p_node = (list_t *)(((uint8_t *)p_elemt) + p_rsc->m_offset);
    add_node(&p_rsc->mp_free_list, p_node);
    --p_rsc->m_used;

    return;

ERR_INVALID_ELEMT:
    fprintf(stderr, "[ERROR] invalid element\n");

    return;
}

void destroy_resource(hixo_resource_t *p_rsc)
{
    if (NULL != p_rsc->mp_start) {
        free(p_rsc->mp_start);
        p_rsc->mp_start = NULL;
    }
    p_rsc->mp_end = NULL;
    p_rsc->mp_free_list = NULL;
    p_rsc->m_used = 0;
    p_rsc->m_capacity = 0;
}
