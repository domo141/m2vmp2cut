#ifndef X_H
#define X_H

typedef enum { false = 0, true = 1 } bool;

#if (__GNUC__ >= 3)
#define GCCATTR_NORETURN __attribute ((noreturn))
#define GCCATTR_CONST    __attribute ((const))
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))

#define from_type(ft, v) \
  __builtin_choose_expr (__builtin_types_compatible_p (typeof (v), ft), \
                       (v), (void)0)

#define checked_cast(tf, ft, v) \
  __builtin_choose_expr (__builtin_types_compatible_p (typeof (v), ft), \
                         ((tt)(v)), (void)0)
#else
#define GCCATTR_NORETURN
#define GCCATTR_CONST
#define GCCATTR_PRINTF(m, n)

#define from_type(ft, v) (v)
#define checked_cast(tt, ft, v) ((tt)(v))
#endif

/* CS == constant string */
#define WriteCS(fd, s) writefully(fd, (const unsigned char *)s, sizeof s - 1)
#define StrCpyCS(d, s) memcpy(d, s, sizeof s)
#define StrCmpCS0(v, c) memcmp(v, c, sizeof c - 1)
#define StrCmpCS(v, c) memcmp(v, c, sizeof c)

/*
 * x.c utility functions.
 */

#include <unistd.h>

ssize_t readfully(int fd, unsigned char * buf, ssize_t len);
ssize_t writefully(int fd, const unsigned char * buf, ssize_t len);

void fdprintf(int fd, const char * format, ...) GCCATTR_PRINTF(2, 3);
void xerrf(const char * format, ...) GCCATTR_NORETURN  GCCATTR_PRINTF(1, 2);

/*
 * debug macros.
 */

#undef D
#if DBGS
#define D(f, m) if (f) printf m
#else
#define D(f, m) do {} while (0)
#endif

#endif /* X_H */
