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


#include "conf.h"
#include "core_module.h"


static hixo_conf_t s_conf = {};
static hixo_core_module_ctx_t s_main_core_ctx = {
    &s_conf,
};

static int main_core_init(void)
{
    int rslt;

    rslt = create_conf(&s_conf);
    if (HIXO_OK == rslt) {
        g_rt_ctx.mp_conf = &s_conf;
    } else {
        g_rt_ctx.mp_conf = NULL;
    }

    return rslt;
}

static void main_core_exit(void)
{
    destroy_conf(&s_conf);

    return;
}

hixo_module_t g_main_core_module = {
    HIXO_MODULE_CORE,
    UNINITIALIZED,
    &main_core_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &main_core_exit,
    &s_main_core_ctx,
};

