#ifndef OBJ_BASE_TYPE_H_
#define OBJ_BASE_TYPE_H_

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t  bool;
#define true  (1 == 1)
#define false (1 == 0)

/* C99 typeof */
#define typeof __typeof__

/*
 * Unused Attribute Macros
 * example:
 *   void foo(int UNUSED(bar)) { ... }
 *   static void UNUSED_FUNCTION(foo)(int bar) { ... }
 */
#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
    #define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
    #define UNUSED(x) UNUSED_ ## x
    #define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

/**
 * Object allocation/initialization macro, using designated initializer.
 */

#if 0
#define INIT(this, ...) do { \
	(this) = malloc(sizeof(*(this))); \
	*(this) = (typeof(*(this))){ __VA_ARGS__ }; \
} while(0)
#endif

#endif
