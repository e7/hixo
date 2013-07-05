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


#include "common.h"
#include "conf.h"


#define DAEMON                  FALSE
#define WORKER_PROCESSES        1
#define MAX_CONNECTIONS         512
#define MAX_EVENTS              512
#define TIMER_RESOLUTION        20
#define CONNECTION_TIME_OUT     60

static hixo_listen_conf_t const S_SRV_ADDRS[] = {
    {INADDR_ANY, 8001, 0},
    {INADDR_ANY, 8002, 0},
    {INADDR_ANY, 8003, 0},
    {INADDR_ANY, 8004, 0},
};

int create_conf(hixo_conf_t *p_conf)
{
    if (ARRAY_COUNT(S_SRV_ADDRS) > MIN(MAX_CONNECTIONS, MAX_EVENTS)) {
        return HIXO_ERROR;
    }

    p_conf->mppc_srv_addrs
        = (hixo_listen_conf_t const **)
        calloc(ARRAY_COUNT(S_SRV_ADDRS) + 1,
               sizeof(hixo_listen_conf_t const *));
    if (NULL == p_conf->mppc_srv_addrs) {
        return HIXO_ERROR;
    }

    p_conf->m_daemon = DAEMON;
    p_conf->m_worker_processes = WORKER_PROCESSES;
    p_conf->m_max_connections = MAX_CONNECTIONS;
    p_conf->m_max_events = MAX_EVENTS;
    p_conf->m_timer_resolution = TIMER_RESOLUTION;
    p_conf->m_connection_timeout = CONNECTION_TIME_OUT;
    p_conf->m_nservers = 0;
    for (int i = 0; i < ARRAY_COUNT(S_SRV_ADDRS); ++i) {
        p_conf->mppc_srv_addrs[i] = &S_SRV_ADDRS[i];
        ++p_conf->m_nservers;
    }
    p_conf->mppc_srv_addrs[ARRAY_COUNT(S_SRV_ADDRS)] = NULL;

    return HIXO_OK;
}

void destroy_conf(hixo_conf_t *p_conf)
{
    if (NULL != p_conf->mppc_srv_addrs) {
        free(p_conf->mppc_srv_addrs);
        p_conf->mppc_srv_addrs = NULL;
    }

    return;
}
