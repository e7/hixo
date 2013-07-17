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


#define HIXO_EVENT_IN           EPOLLIN
#define HIXO_EVENT_OUT          EPOLLOUT
#define HIXO_EVENT_ERR          EPOLLERR
#define HIXO_EVENT_HUP          EPOLLHUP
#define HIXO_EVENT_FLAGS        EPOLLET


typedef struct {
    int (*mpf_init)(void);
    int (*mpf_add_event)(hixo_socket_t *);
    int (*mpf_mod_event)(void);
    int (*mpf_del_event)(hixo_socket_t *);
    int (*mpf_process_events)(int);
    void (*mpf_exit)(void);

    int m_initialized;
    void *mp_private;
} hixo_event_module_ctx_t;


extern void hixo_accept_handler(void);


extern hixo_module_t g_epoll_module;
#endif // __EVENT_H__
