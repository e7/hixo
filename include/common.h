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


#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#define FALSE           0
#define TRUE            (!FALSE)

typedef unsigned char byte_t;

#define ARRAY_COUNT(a)      (sizeof(a) / sizeof(a[0]))
#define OFFSET_OF(s, m)     ((intptr_t)&(((s *)0)->m ))
#define CONTAINER_OF(ptr, type, member)     \
            ({\
                const typeof(((type *)0)->member) *p_mptr = (ptr);\
                (type *)((byte_t *)p_mptr - OFFSET_OF(type, member));\
            })


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __COMMON_H__
