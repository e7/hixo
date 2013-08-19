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
#define MAX_CONNECTIONS         10000
#define TIMER_RESOLUTION        1000
#define CONNECTION_TIME_OUT     60
#define LINGER_TIME_OUT         10

int create_conf(hixo_conf_t *p_conf)
{
    p_conf->m_daemon = DAEMON;
    p_conf->m_worker_processes = WORKER_PROCESSES;
    p_conf->m_max_connections = MAX_CONNECTIONS;
    p_conf->m_timer_resolution = TIMER_RESOLUTION;
    p_conf->m_connection_timeout = CONNECTION_TIME_OUT;

    return HIXO_OK;
}

void destroy_conf(hixo_conf_t *p_conf)
{
    return;
}
