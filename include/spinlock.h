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


#ifndef __SPINLOCK__
#define __SPINLOCK__

#include "common.h"

#if __cplusplus
extern "C" {
#endif // __cplusplus

static inline
int atomic_cmp_set(atomic_t *lock, uint32_t old, uint32_t set)
{
    uint8_t rslt = 0;

    assert(NULL != lock);
    __asm__ __volatile__ ("lock;" // lock if SMP
                          "cmpxchgl %3, %1;"
                          "sete %0;"
                          : "=a" (rslt)
                          : "m" (*lock), "a" (old), "r" (set)
                          : "cc", "memory");

    return rslt;
}

#define spinlock_try(lock) atomic_cmp_set(lock, 0, 1)
#define spinlock_unlock(lock) (*lock = 0)

#if __cplusplus
}
#endif // __cplusplus
#endif //__SPINLOCK__
