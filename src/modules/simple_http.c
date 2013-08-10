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


#include "app_module.h"



static hixo_app_module_ctx_t s_simple_http_ctx = {
};

hixo_module_t g_simple_http_module = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    HIXO_MODULE_APP,
    &s_simple_http_ctx,
};
