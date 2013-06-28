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


#include "core_module.h"


static hixo_core_module_ctx_t s_event_core_ctx = {};


static int event_core_init(void)
{
    fprintf(stderr, "i am event_core_module\n");
    return HIXO_OK;
}

static void event_core_exit(void)
{
}


hixo_module_t g_event_core_module = {
    HIXO_MODULE_CORE,
    &s_event_core_ctx,
    &event_core_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &event_core_exit,
};
