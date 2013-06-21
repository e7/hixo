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


#ifndef __BITMAP_H__
#define __BITMAP_H__


#include "hixo.h"

typedef struct {
    uint8_t *mp_data;
    uint32_t m_size;
} bitmap_t;

static inline
int create_bitmap(bitmap_t *p_bm, uint32_t nbits)
{
    if (0 == nbits) {
        return HIXO_ERROR;
    }

    p_bm->m_size = (nbits - 1) / 8 + 1;
    p_bm->mp_data = (uint8_t *)calloc(1, p_bm->m_size);
    if (NULL == p_bm->mp_data) {
        return HIXO_ERROR;
    }
    (void)memset(p_bm->mp_data, 0, p_bm->m_size);

    return HIXO_OK;
}

static inline
int bitmap_set(bitmap_t *p_bm, uint32_t bit_offset)
{
    uint32_t byte_offset = bit_offset / 8;
    uint32_t byte_bit_offset = bit_offset % 8;

    if (bit_offset > (8 * p_bm->m_size - 1)) {
        fprintf(stderr, "[ERROR] bitmap bit_offset out of range\n");

        return HIXO_ERROR;
    }

    p_bm->mp_data[byte_offset] |= 1 << byte_bit_offset;

    return HIXO_OK;
}

static inline
int bitmap_is_set(bitmap_t *p_bm, uint32_t bit_offset)
{
    uint32_t byte_offset = bit_offset / 8;
    uint32_t byte_bit_offset = bit_offset % 8;

    if (bit_offset > (8 * p_bm->m_size - 1)) {
        fprintf(stderr, "[ERROR] bitmap bit_offset out of range\n");

        return FALSE;
    }

    if (p_bm->mp_data[byte_offset] & (1 << byte_bit_offset))
    {
        return TRUE;
    } else {
        return FALSE;
    }

}

static inline
void destroy_bitmap(bitmap_t *p_bm)
{
    if (NULL != p_bm->mp_data) {
        free(p_bm->mp_data);
        p_bm->mp_data = NULL;
    }
    p_bm->m_size = 0;
}
#endif // __BITMAP_H__
