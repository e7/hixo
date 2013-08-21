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


#include "buffer.h"


void hixo_create_byte_array(hixo_array_t *p_byta_array, ssize_t capacity)
{
    if (capacity > 0) {
        p_byta_array->mp_data = (uint8_t *)calloc(1, capacity);
        p_byta_array->m_capacity = capacity;
    } else {
        p_byta_array->mp_data = NULL;
        p_byta_array->m_capacity = 0;
    }
}

void hixo_byte_array_transfer(hixo_array_t *p_recv,
                              hixo_array_t *p_send)
{
    if (NULL != p_recv->mp_data) {
        free(p_recv->mp_data);
    }
    p_recv->mp_data = p_send->mp_data;
    p_recv->m_capacity = p_send->m_capacity;
    p_send->mp_data = NULL;
    p_send->m_capacity = 0;
}

void hixo_destroy_byte_array(hixo_array_t *p_byta_array)
{
    if (NULL != p_byta_array->mp_data) {
        free(p_byta_array->mp_data);
        p_byta_array->mp_data = NULL;
    }
}


int hixo_create_buffer(hixo_buffer_t *p_buf, ssize_t capacity)
{
    assert(NULL == p_buf->m_byte_array.mp_data);
    if (capacity < 0) {
        goto ERROR;
    } else if (capacity > 0) {
        hixo_create_byte_array(&p_buf->m_byte_array, capacity);
        p_buf->m_offset = 0;
        p_buf->m_size = 0;
        dlist_init(&p_buf->m_node);
    } else {
        hixo_create_byte_array(&p_buf->m_byte_array, 0);
        p_buf->m_offset = 0;
        p_buf->m_size = 0;
        dlist_init(&p_buf->m_node);
    }

    return HIXO_OK;

ERROR:
    return HIXO_ERROR;
}

void hixo_expand_buffer(hixo_buffer_t *p_buf)
{
    hixo_array_t tmp_array;
    ssize_t data_size = 2 * p_buf->m_byte_array.m_capacity;

    assert(data_size > 0);
    hixo_create_byte_array(&tmp_array, data_size);
    hixo_byte_array_transfer(&p_buf->m_byte_array, &tmp_array);

    return;
}

void hixo_destroy_buffer(hixo_buffer_t *p_buf)
{
    hixo_destroy_byte_array(&p_buf->m_byte_array);

    return;
}
