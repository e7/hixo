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


#ifndef __HIXO_H__
#define __HIXO_H__


#include "conf.h"
#include "spinlock.h"
#include "resource.h"
#include "socket.h"
#include "list.h"
#include "buffer.h"


typedef enum {
    HIXO_MODULE_CORE = 0xE78F8A,
    HIXO_MODULE_EVENT,
    HIXO_MODULE_APP,
} hixo_module_type_t;

typedef struct {
    char *mp_ip;
    uint16_t m_port;
    int m_backlog;
} hixo_listen_conf_t;

typedef struct {
    int (*mpf_init_master)(void);
    int (*mpf_init_worker)(void);
    int (*mpf_init_thread)(void);
    void(*mpf_exit_thread)(void);
    void (*mpf_exit_worker)(void);
    void (*mpf_exit_master)(void);

    hixo_module_type_t m_type;
    void *mp_ctx;
} hixo_module_t;

typedef struct {
    int const M_PAGE_SIZE;
    int const M_MAX_FILE_NO;
    int const M_NCPUS;
    int const M_MAX_PSS;
} hixo_sysconf_t;

typedef struct {
    hixo_conf_t *mp_conf;
    atomic_t *mp_accept_lock;
    hixo_socket_t **mpp_servers;
    int m_nservers;
    list_t *mp_connections;
    list_t *mp_posted_events;
    hixo_resource_t m_sockets_cache;
} hixo_rt_context_t;

typedef struct {
    pid_t m_pid;
    intptr_t m_cpuid;
    int m_power;
    int m_tunnel[2];
} hixo_ps_info_t;

typedef enum {
    HIXO_STATUS_RUNNING = 0xe78f8a,
    HIXO_STATUS_QUIT,
    HIXO_STATUS_CHILD,
    HIXO_STATUS_INT,
    HIXO_STATUS_HUP,
} hixo_status_t;


extern hixo_sysconf_t g_sysconf;
extern hixo_module_t *gap_modules[];
extern hixo_rt_context_t g_rt_ctx;
extern hixo_ps_info_t ga_pss_info[];
extern hixo_ps_info_t *gp_ps_info;
extern hixo_status_t g_status;
#endif // __HIXO_H__
