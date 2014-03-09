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


#ifndef __SLUB_H__
#define __SLUB_H__


#include "common.h"


extern void mem_shift_left(uint8_t *p, intptr_t len, intptr_t n);
extern int make_slub(void *p, intptr_t size);
extern void dump_mem(void *p, intptr_t size);
extern void dump_slub(void *p);
#endif // __HIXO_SLUB_H__
