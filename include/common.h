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


#ifndef __COMMON_H__
#define __COMMON_H__

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <execinfo.h>
#include <sched.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#define __UINT_ZOOO__ (~(uintptr_t)0 >> 1)
#define INTEGER_MAX ((intptr_t)__UINT_ZOOO__)
#define INTEGER_MIN ((intptr_t)~__UINT_ZOOO__)

#if BITS_32
#define STR_INTEGER_MAX "2147483647"
#define STR_INTEGER_MIN "-2147483648"
#else
#define STR_INTEGER_MAX "9223372036854775807"
#define STR_INTEGER_MIN "-9223372036854775808"
#endif


typedef volatile uintptr_t atomic_t;

typedef struct {
    uint8_t *mp_data;
    ssize_t m_capacity;
} hixo_array_t;
extern void hixo_create_byte_array(hixo_array_t *p_byta_array,
                                   ssize_t capacity);
extern void hixo_byte_array_transfer(hixo_array_t *p_recv,
                                     hixo_array_t *p_send);
extern void hixo_destroy_byte_array(hixo_array_t *p_byta_array);


#define FALSE           0
#define TRUE            (!FALSE)

#define do_nothing()    do {} while (0)

#define ARRAY_COUNT(a)      (sizeof(a) / sizeof(a[0]))
#define OFFSET_OF(s, m)     ((size_t)&(((s *)0)->m ))
#define CONTAINER_OF(ptr, type, member)     \
            ({\
                const __typeof__(((type *)0)->member) *p_mptr = (ptr);\
                (type *)((uint8_t *)p_mptr - OFFSET_OF(type, member));\
             })

#define MIN(a, b)           (((a) > (b)) ? (b) : (a))
#define MAX(a, b)           (((a) < (b)) ? (b) : (a))

static inline
intptr_t abs_value(intptr_t num)
{
    if (INTEGER_MIN == num) {
        return -1;
    } else if (num > 0) {
        return num;
    } else {
        return -num;
    }
}

static inline
intptr_t count_places(intptr_t num)
{
    static intptr_t const places_max[] = {
        9, 99, 999, 9999, 99999, // 1-5
        999999, // 6
        9999999,
        99999999,
        999999999, // 9
    #if !BITS_32
        9999999999, // 10
        99999999999,
        999999999999,
        9999999999999, // 13
        99999999999999,
        999999999999999, // 15
        9999999999999999,
        99999999999999999,
        999999999999999999, // 18
    #endif
    };

    intptr_t rslt = 0;
    intptr_t start = 0;
    intptr_t middle = 0;
    intptr_t end = ARRAY_COUNT(places_max);
    intptr_t positive = 0;

    if (INTEGER_MIN == num) {
        positive = abs_value(num + 1);
    } else {
        positive = abs_value(num);
    }

    while (start < end) {
        middle = start + (end - start) / 2;
        if (positive <= places_max[middle]) {
            rslt = middle + 1;
            end = middle;
        } else {
            rslt = middle + 2;
            start = middle + 1;
        }
    }

    if (num < 0) {
        ++rslt;
    }

    return rslt;
}

static inline
void __str_reverse__(char str[], ssize_t len)
{
    for (int i = 0, j = len - 1; i < j; ++i, --j) {
        char tmp = str[i];

        str[i] = str[j];
        str[j] = tmp;
    }
}

static inline
char const *wtoa(intptr_t num, ssize_t places, char buf[], ssize_t len)
{
    intptr_t value = abs_value(num);
    ssize_t end = 0;

    if (places > len - 1) {
        return NULL;
    }

    if (INTEGER_MIN == num) {
        (void)strncpy(buf, STR_INTEGER_MIN, sizeof(STR_INTEGER_MIN) - 1);

        return buf;
    }

    if (0 == value) {
        buf[end++] = '0';
    } else {
        while (value > 0) {
            intptr_t next_value = value / 10;

            buf[end++] = value - next_value * 10 + '0';
            value = next_value;
        }

        if (num < 0) {
            buf[end++] = '-';
        }

        __str_reverse__(buf, end);
    }
    buf[end] = 0;

    return buf;
}

#ifndef ESUCCESS
    #define ESUCCESS        (0)
#endif
#define HIXO_OK             (0)
#define HIXO_ERROR          (-1)
#define INVALID_FD          (~0)

#define unblocking_fd(fd)   fcntl(fd, \
                                  F_SETFL, \
                                  fcntl(fd, F_GETFL) | O_NONBLOCK)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __COMMON_H__
