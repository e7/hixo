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


#ifndef __BUFFER_H__
#define __BUFFER_H__


#include "list.h"


typedef struct {
    uint8_t *mp_data;
    ssize_t m_offset;
    ssize_t m_size;
    ssize_t m_capacity;
    dlist_t m_node;
} hixo_buffer_t;


static inline
uint8_t *hixo_buffer_get_data(hixo_buffer_t *p_buf)
{
    return p_buf->mp_data;
}

static inline
int hixo_buffer_full(hixo_buffer_t *p_buf)
{
    return (p_buf->m_size >= p_buf->m_capacity);
}

static inline
void hixo_buffer_clean(hixo_buffer_t *p_buf)
{
    return;
}

static inline
void hixo_buffer_set_size(hixo_buffer_t *p_buf, ssize_t size)
{
    size = (size > 0) ? size : 0;
    p_buf->m_size = size;

    return;
}

static inline
ssize_t hixo_buffer_get_size(hixo_buffer_t *p_buf)
{
    return p_buf->m_size;
}

static inline
ssize_t hixo_buffer_get_capacity(hixo_buffer_t *p_buf)
{
    return p_buf->m_capacity;
}

extern int hixo_create_buffer(hixo_buffer_t *p_buf, ssize_t capacity);
extern int hixo_expand_buffer(hixo_buffer_t *p_buf);
extern void hixo_destroy_buffer(hixo_buffer_t *p_buf);
#endif // __BUFFER_H__
