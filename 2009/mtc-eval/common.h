#ifndef COMMONH
#define COMMONH

#include <stdio.h>

#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

#define UNDEF   -1

#define MAX(A,B)  ((A) > (B) ? (A) : (B))
#define MIN(A,B)  ((A) > (B) ? (B) : (A))

#ifndef MAXLONG
#define MAXLONG 2147483647L             /* largest long int. no. */
#endif

/*
 * Some useful macros for making malloc et al easier to use.
 * Macros handle the casting and the like that's needed.
 */
#define tMalloc(n,type) (type *) malloc( (unsigned) ((n)*sizeof(type)))
#define tRealloc(loc,n,type) (type *) realloc( (char *)(loc), \
                                              (unsigned) ((n)*sizeof(type)))
#define tFree(loc) (void) free( (char *)(loc) )

#endif /* COMMONH */
