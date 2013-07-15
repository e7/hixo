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


int hixo_create_buffer(hixo_buffer_t *p_buf, ssize_t capacity)
{
    int rslt;
    uint8_t *p_data;

    p_data = (uint8_t *)calloc(1, capacity);
    if (NULL == p_data) {
        goto ERR_OUT_OF_MEM;
    }

    p_buf->mp_buf = p_data;
    p_buf->m_offset = 0;
    p_buf->m_size = 0;
    p_buf->m_capacity = capacity;

    do {
        rslt = HIXO_OK;
        break;

ERR_OUT_OF_MEM:
        rslt = HIXO_ERROR;
        break;
    } while (0);

    return rslt;
}

void hixo_destroy_buffer(hixo_buffer_t *p_buf)
{
    free(p_buf->mp_buf);
    p_buf->mp_buf = NULL;

    return;
}
