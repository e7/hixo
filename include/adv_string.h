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


#ifndef __ADV_STRING__H__
#define __ADV_STRING__H__


#include "common.h"


typedef struct {
    char *mp_str;
    ssize_t m_size;
    hixo_array_t *mp_buf;
} hixo_adv_string_t;
#define INIT_ADV_STRING(orig)               {orig, sizeof(orig) - 1, NULL}
#define DEFINE_ADV_STRING(name, orig)       \
            hixo_adv_string_t name = INIT_ADV_STRING(orig)

#endif // __ADV_STRING__H__
