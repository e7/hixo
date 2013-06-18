#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#define FALSE           0
#define TRUE            (!FALSE)

#define ARRAY_COUNT(a)  (sizeof(a) / sizeof(a[0]))


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __COMMON_H__
