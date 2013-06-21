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


#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common.h"


#ifndef __HIXO_H__
#define __HIXO_H__

#ifndef ESUCCESS
    #define ESUCCESS        (0)
#endif
#define HIXO_OK             (0)
#define HIXO_ERROR          (-1)
#define unblocking_fd(fd)   fcntl(fd, \
                                  F_SETFL, \
                                  fcntl(fd, F_GETFL) | O_NONBLOCK)

// config {{
typedef struct {
    uint32_t m_ip;
    uint16_t m_port;
    int m_backlog;
} addr_t;

#define DAEMON                  FALSE
#define WORKER_PROCESSES        4
#define MAX_CONNECTIONS         512
#define MAX_EVENTS              512
#define TIMER_RESOLUTION        20
#define CONNECTION_TIME_OUT     60
static addr_t const SRV_ADDRS[] = {
    {INADDR_ANY, 80,   0},
    {INADDR_ANY, 8888, 0},
    {INADDR_ANY, 8889, 0},
    {INADDR_ANY, 8890, 0},
};
#define SRV_ADDRS_COUNT         ARRAY_COUNT(SRV_ADDRS)
// }} config

typedef enum {
    HIXO_CORE,
    HIXO_EVENT,
} hixo_module_type_t;

typedef struct {
    hixo_module_type_t m_type;
    void *mp_ctx;
} hixo_module_t;
#endif // __HIXO_H__
