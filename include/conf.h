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


#ifndef __CONF_H__
#define __CONF_H__

#include "common.h"


typedef struct {
    uint32_t m_ip;
    uint16_t m_port;
    int m_backlog;
} hixo_listen_conf_t;

typedef struct s_conf_t {
    int m_daemon;
    int m_worker_processes;
    int m_max_connections;
    int m_max_events;
    int m_timer_resolution;
    int m_connection_timeout;
    hixo_listen_conf_t const **mppc_srv_addrs;
    int m_nservers;
} hixo_conf_t;

extern int create_conf(hixo_conf_t *p_conf);
extern void destroy_conf(hixo_conf_t *p_conf);
#endif // __CONF_H__
