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

#include "list.h"

extern hixo_module_t g_epoll_module;

typedef enum {
    CLOSED = 0x00000001,
    OPENED = (CLOSED << 1) + 1,
    CONFIGURED = OPENED + 1,
    BOUND = CONFIGURED + 1,
    LISTENING = BOUND + 1,
    CONNECTED = (CLOSED << 2) + 1,
    BROKEN = (CLOSED << 3) + 1,
} socket_status_t;

typedef struct s_hixo_event_t hixo_event_t;
struct s_hixo_event_t {
    int m_fd;
    uint32_t m_ev_flags;
    int m_overdue;
    int (*mpf_read)(hixo_event_t *);
    int (*mpf_write)(hixo_event_t *);
};

typedef struct s_hixo_socket_t hixo_socket_t;
struct s_hixo_socket_t {
    socket_status_t m_status;
    hixo_event_t m_event;
};

typedef struct {
    int (*mpf_init)(void);
    void (*mpf_add_event)(hixo_event_t *);
    int (*mpf_mod_event)(void);
    int (*mpf_del_event)(void);
    int (*mpf_process_events)(void);
    void (*mpf_uninit)(void);

    int m_fd;
    list_t *mp_shut_read_list;
    list_t *mp_shut_write_list;
    void *mp_misc;
} hixo_event_module_ctx_t;
#endif // __EVENT_H__
