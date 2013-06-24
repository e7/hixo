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
    int (*mpf_read)(void);
    int (*mpf_write)(void);
};

typedef struct {
    hixo_socket_t *mp_listeners;
} hixo_rt_context_t;

extern hixo_rt_context_t g_rt_ctx;

// modules
extern hixo_module_t g_main_core_module;
extern hixo_module_t g_epoll_module;
#endif // __HIXO_H__
