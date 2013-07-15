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
#include "conf.h"
#include "spinlock.h"
#include "buffer.h"


#ifndef __HIXO_H__
#define __HIXO_H__


typedef enum {
    HIXO_MODULE_CORE,
    HIXO_MODULE_EVENT,
    HIXO_MODULE_APP,
} hixo_module_type_t;


typedef struct s_socket_t hixo_socket_t;


// hixo_resource_t {{
typedef struct {
    void *mp_start;
    void *mp_end;
    int m_offset; // offset of list node
    list_t *mp_free_list;
    int m_used;
    int m_capacity;
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

static inline
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

static inline
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

static inline
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
// }} hixo_resource_t


struct s_socket_t {
    int m_fd;
    void (*mpf_read_handler)(hixo_socket_t *);
    void (*mpf_write_handler)(hixo_socket_t *);
    list_t m_node;
    int m_event_types;
    hixo_buffer_t m_readbuf;
    hixo_buffer_t m_writebuf;
    unsigned int m_active : 1;
    unsigned int m_exists : 1;
    unsigned int m_stale : 1;
    unsigned int m_readable : 1;
    unsigned int m_writable : 1;
};


typedef enum {
    UNINITIALIZED = 0xE78F8A,
    MASTER_INITIALIZED = UNINITIALIZED + 1,
    WORKER_INITIALIZED = MASTER_INITIALIZED + 1,
    THREAD_INITIALIZED = WORKER_INITIALIZED + 1,
} hixo_module_status_t;
typedef struct {
    hixo_module_type_t m_type;
    int (*mpf_init_master)(void);
    int (*mpf_init_worker)(void);
    int (*mpf_init_thread)(void);
    void(*mpf_exit_thread)(void);
    void (*mpf_exit_worker)(void);
    void (*mpf_exit_master)(void);
    void *mp_ctx;
} hixo_module_t;

typedef struct {
    hixo_conf_t *mp_conf;
    void *mp_ctx;
    atomic_t *mp_accept_lock;
    hixo_socket_t **mpp_listeners;
    list_t *mp_connections;
    hixo_resource_t m_sockets_cache;
} hixo_rt_context_t;

typedef struct {
    int m_master;
    int m_power;
} hixo_ps_status_t;


extern hixo_rt_context_t g_rt_ctx;
extern hixo_ps_status_t g_ps_status;
#endif // __HIXO_H__
