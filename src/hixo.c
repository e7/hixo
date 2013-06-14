#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"


// config {{
#define DAEMON                  FALSE
#define SRV_PORT                8888
#define WORKER_PROCESSES        4
#define MAX_CONNECTIONS         64
// }} config

#define unblocking_fd(fd)   fcntl(fd, \
                                  F_SETFL, \
                                  fcntl(fd, F_GETFL) | O_NONBLOCK)
typedef enum {
    HIXO_E_IN,
    HIXO_E_OUT,
} hixo_event_type_t;

typedef struct {
    hixo_event_type_t m_ev_type;
    void (*mpf_handler)(void *);
} hixo_event_t;

typedef enum {
    HIXO_CORE,
    HIXO_EVENT,
} hixo_module_type_t;

typedef struct {
    int m_fd;
    int (*mpf_init)(void);
    int (*mpf_add_event)(void);
    int (*mpf_mod_event)(void);
    int (*mpf_del_event)(void);
    void (*mpf_uninit)(void);
} hixo_event_ctx_t;

typedef struct {
    hixo_module_type_t m_type;
} hixo_module_t;

// epoll模块
hixo_event_ctx_t g_epoll_module_ctx = {
    0,
};
hixo_module_t g_epoll_module = {
    HIXO_EVENT,
};

hixo_module_t *g_modules[] = {
    &g_epoll_module,
};


int g_master = TRUE;

static int master_main(void)
{
    return 0;
}

static int worker_main(int sock)
{
    int epfd = 0;
    int tmp_err = 0;

    errno = 0;
    epfd = epoll_create(MAX_CONNECTIONS);
    tmp_err = errno;
    if (-1 == epfd) {
        goto ERR_EPOLL_CREATE;
    }

ERR_EPOLL_CREATE:

    return 0;
}

static int hixo_main(int sock)
{
    if (DAEMON) {
    }

    for (int i = 0; i < WORKER_PROCESSES; ++i) {
        pid_t cpid = fork();
        if (-1 == cpid) {
            return -1;
        } else if (0 == cpid) {
            g_master = FALSE;

            break;
        } else {
            g_master = TRUE;
        }
    }

    if (g_master) {
        return master_main();
    } else {
        return worker_main(sock);
    }
}

int main(int argc, char *argv[])
{
    int rslt = EXIT_FAILURE;
    int lsn_fd = 0;
    int tmp_err = 0;
    struct sockaddr_in srv_addr = {};

    // 创建套接字
    errno = 0;
    lsn_fd = socket(PF_INET, SOCK_STREAM, 0);
    tmp_err = errno;
    if (-1 == lsn_fd) {
        fprintf(stderr, "[ERROR] socket() failed: %d\n", tmp_err);

        goto ERR_SOCKET;
    }

    // 非阻塞
    errno = 0;
    if (-1 == unblocking_fd(lsn_fd)) {
        tmp_err = errno;
        fprintf(stderr, "[ERROR] fcntl() failed: %d\n", tmp_err);

        goto ERR_FCNTL;
    }

    // 绑定
    (void)memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port = htons(SRV_PORT);
    errno = 0;
    if (-1 == bind(lsn_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr)))
    {
        tmp_err = errno;
        fprintf(stderr, "[ERROR] bind() failed: %d\n", tmp_err);

        goto ERR_BIND;
    }

    // 监听
    errno = 0;
    if (-1 == listen(lsn_fd, SOMAXCONN)) {
        tmp_err = errno;
        fprintf(stderr, "[ERROR] listen failed: %d\n", tmp_err);

        goto ERR_LISTEN;
    }

    rslt = (0 == hixo_main(lsn_fd)) ? EXIT_SUCCESS : EXIT_FAILURE;

    // 错误处理
ERR_LISTEN:

ERR_BIND:

ERR_FCNTL:
    (void)close(lsn_fd);

ERR_SOCKET:

    return rslt;
}
