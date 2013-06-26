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


#include "list.h"
#include "bitmap.h"


#ifndef __HIXO_H__
#define __HIXO_H__


typedef enum {
    HIXO_MODULE_CORE,
    HIXO_MODULE_EVENT,
} hixo_module_type_t;

// hixo_resource_t {{
typedef struct {
    void *mp_data;
    list_t *mp_inuse_list;
    list_t *mp_free_list;
} hixo_resource_t;

static inline
int create_resource(hixo_resource_t *p_rsc,
                    size_t count,
                    size_t elemt_size,
                    size_t offset)
{
    list_t *p_node = NULL;

    if ((0 == count) || (0 == elemt_size)) {
        return HIXO_ERROR;
    }

    if (NULL != p_rsc->mp_data) {
        fprintf(stderr, "[ERROR] resource exists\n");

        return HIXO_ERROR;
    }

    // start
    p_rsc->mp_data = calloc(count, elemt_size);
    if (NULL == p_rsc->mp_data) {
        fprintf(stderr, "[ERROR} out of memory\n");

        return HIXO_ERROR;
    }
    p_rsc->mp_inuse_list = NULL;
    p_rsc->mp_free_list = NULL;
    for (int i = 0; i < count; ++i) {
        p_node = (list_t *)(((uint8_t *)p_rsc->mp_data)
                                + i * elemt_size
                                + offset);
        add_node(&p_rsc->mp_free_list, p_node);
    }

    return HIXO_OK;
}

static inline
void destroy(hixo_resource_t *p_rsc)
{
    if (NULL != p_rsc->mp_data) {
        free(p_rsc->mp_data);
        p_rsc->mp_data = NULL;
    }
    p_rsc->mp_inuse_list = NULL;
    p_rsc->mp_free_list = NULL;
}
// }} hixo_resource_t


typedef struct {
    hixo_module_type_t m_type;
    void *mp_ctx;
    int (*mpf_init_master)(void);
    int (*mpf_init_worker)(void);
    void (*mpf_exit_worker)(void);
    void (*mpf_exit_master)(void);
} hixo_module_t;

typedef struct s_socket_t hixo_socket_t;
struct s_socket_t {
    int m_fd;
    list_t m_node;
};

typedef struct {
    hixo_socket_t *mp_listeners;
    hixo_resource_t m_sockets;
    hixo_resource_t m_events;
} hixo_rt_context_t;

extern hixo_rt_context_t g_rt_ctx;

// modules
extern hixo_module_t g_main_core_module;
extern hixo_module_t g_epoll_module;
#endif // __HIXO_H__
