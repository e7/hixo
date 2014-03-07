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


#include "slub.h"


#define BLOCK_SIZE 4096


static intptr_t slub_objs_shift[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, // valid 8--2048
};


int make_slub(void *p, intptr_t size)
{
    int rslt = 0;
    intptr_t shift = 0;
    intptr_t bitmap = 0;

    assert(NULL != p);
    assert(size > 0);

    if (size - BLOCK_SIZE < BLOCK_SIZE) {
        goto ERROR;
    }

    shift = (size - BLOCK_SIZE) / BLOCK_SIZE;
    bitmap = (shift + sizeof(char)) / sizeof(char);

    do {
        break;
ERROR:
        rslt = -1;
    } while (0);

    return rslt;
}
