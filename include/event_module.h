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


#ifndef __EVENT_H__
#define __EVENT_H__

#include "hixo.h"


struct s_event_t {
    void (*mpf_read_handler)(hixo_socket_t *);
    void (*mpf_write_handler)(hixo_socket_t *);
    list_t m_node;
    void *mp_data;
    unsigned int m_active : 1;
    unsigned int m_stale : 1;
};

typedef struct {
    int (*mpf_init)(void);
    void (*mpf_add_event)(hixo_event_t *, uint32_t, uint32_t);
    int (*mpf_mod_event)(void);
    void (*mpf_del_event)(hixo_event_t *, uint32_t, uint32_t);
    int (*mpf_process_events)(int);
    void (*mpf_exit)(void);

    int m_initialized;
    void *mp_private;
} hixo_event_module_ctx_t;


extern void hixo_accept_handler(void);


extern hixo_module_t g_epoll_module;
#endif // __EVENT_H__
