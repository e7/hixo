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


#ifndef __APP_MODULE_H__
#define __APP_MODULE_H__


#include "hixo.h"


typedef struct {
    void (*mpf_handle_connect)(hixo_socket_t *);
    void (*mpf_handle_read)(hixo_socket_t *);
    void (*mpf_handle_write)(hixo_socket_t *);
    void (*mpf_handle_close)(hixo_socket_t *);
} hixo_app_module_ctx_t;


extern hixo_module_t g_simple_http_module;
#endif // __APP_MODULE_H__
