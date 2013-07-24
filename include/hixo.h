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


#include "conf.h"
#include "spinlock.h"
#include "resource.h"
#include "socket.h"
#include "list.h"
#include "buffer.h"


#ifndef __HIXO_H__
#define __HIXO_H__


typedef enum {
    HIXO_MODULE_CORE,
    HIXO_MODULE_EVENT,
    HIXO_MODULE_APP,
} hixo_module_type_t;

typedef struct {
    int (*mpf_init_master)(void);
    int (*mpf_init_worker)(void);
    int (*mpf_init_thread)(void);
    void(*mpf_exit_thread)(void);
    void (*mpf_exit_worker)(void);
    void (*mpf_exit_master)(void);

    hixo_module_type_t m_type;
    dlist_t m_node;

    void *mp_ctx;
} hixo_module_t;

typedef struct {
    int const M_PAGE_SIZE;
    int const M_MAX_FILE_NO;
    int const M_NCPUS;
} hixo_sysconf_t;

typedef struct {
    hixo_conf_t *mp_conf;
    void *mp_ctx;
    atomic_t *mp_accept_lock;
    hixo_socket_t **mpp_listeners;
    list_t *mp_connections;
    list_t *mp_posted_events;
    hixo_resource_t m_sockets_cache;
} hixo_rt_context_t;

typedef struct {
    int m_master;
    int m_power;
} hixo_ps_status_t;


extern hixo_sysconf_t g_sysconf;
extern hixo_rt_context_t g_rt_ctx;
extern hixo_ps_status_t g_ps_status;
#endif // __HIXO_H__
