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


#include "adv_string.h"

int strreverse(char *p_str, ssize_t start, ssize_t end)
{
    int rslt;

    if ((NULL == p_str) || (start < 0) || (end < 0)) {
        return -1;
    }

    if (start > end) {
        rslt = -1;
    } else if (start < end) {
        char tmp;

        for (ssize_t i = start, j = end - 1; i < j; ++i, --j) {
            tmp = p_str[i];
            p_str[i] = p_str[j];
            p_str[j] = tmp;
        }

        rslt = 0;
    } else {
        rslt = 0;
    }

    return rslt;
}

int itostr(intptr_t num, char *p_rslt, ssize_t n)
{
    ssize_t start;
    ssize_t end;

    if ((NULL == p_rslt) || ((n - 1) < 1)) {
        return -1;
    }

    if (0 == num) {
        p_rslt[0] = '0';
        p_rslt[1] = '\0';

        return 0;
    }

    if (num < 0) {
        p_rslt[0] = '-';
        start = 1;
        num = -num;
    } else {
        start = 0;
    }
    end = start;

    while (num && (--n)) {
        char x = (char)(num % 10);

        p_rslt[end++] = '0' + x;
        num /= 10;
    }
    (void)strreverse(p_rslt, start, end);
    p_rslt[end] = '\0';

    return 0;
}

int strtoi(char const *pc_str, ssize_t n, intptr_t *p_rslt)
{
    if ((NULL == pc_str) || (NULL == p_rslt) || (n < 1)) {
        return -1;
    }

    return 0;
}
