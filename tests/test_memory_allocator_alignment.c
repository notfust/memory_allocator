#include "../includes/memory_allocator.h"

#include <assert.h>
#include <complex.h>
#include <stddef.h>
#include <stdint.h>

typedef struct pointer_alignment_probe {
    char  prefix;
    void *value;
} pointer_alignment_probe_t;

typedef struct long_long_alignment_probe {
    char      prefix;
    long long value;
} long_long_alignment_probe_t;

typedef struct long_double_alignment_probe {
    char        prefix;
    long double value;
} long_double_alignment_probe_t;

typedef struct complex_alignment_probe {
    char                 prefix;
    long double _Complex value;
} complex_alignment_probe_t;

#define POINTER_ALIGNMENT offsetof(pointer_alignment_probe_t, value)
#define LONG_LONG_ALIGNMENT offsetof(long_long_alignment_probe_t, value)
#define LONG_DOUBLE_ALIGNMENT offsetof(long_double_alignment_probe_t, value)
#define COMPLEX_ALIGNMENT offsetof(complex_alignment_probe_t, value)

#ifndef TRICORE_TARGET
static void assert_host_pointer_alignment(const void *pointer)
{
    assert((((uintptr_t)pointer) % POINTER_ALIGNMENT) == 0U);
    assert((((uintptr_t)pointer) % LONG_LONG_ALIGNMENT) == 0U);
    assert((((uintptr_t)pointer) % LONG_DOUBLE_ALIGNMENT) == 0U);
    assert((((uintptr_t)pointer) % COMPLEX_ALIGNMENT) == 0U);
}
#else
static void assert_target_pointer_alignment(const void *pointer)
{
    assert((((uintptr_t)pointer) % 4U) == 0U);
}
#endif

int main(void)
{
    void *first;
    void *second;
    void *third;

#ifdef TRICORE_TARGET
    assert(MEMORY_ALLOCATOR_ALIGNMENT == 4U);
#endif

#ifndef TRICORE_TARGET
    assert((MEMORY_ALLOCATOR_ALIGNMENT % POINTER_ALIGNMENT) == 0U);
    assert((MEMORY_ALLOCATOR_ALIGNMENT % LONG_LONG_ALIGNMENT) == 0U);
    assert((MEMORY_ALLOCATOR_ALIGNMENT % LONG_DOUBLE_ALIGNMENT) == 0U);
    assert((MEMORY_ALLOCATOR_ALIGNMENT % COMPLEX_ALIGNMENT) == 0U);
#endif

    memory_init();

    first = memory_alloc(1U);
    second = memory_alloc(sizeof(long double));
    third = memory_alloc(sizeof(long double _Complex));

    assert(first != NULL);
    assert(second != NULL);
    assert(third != NULL);

#ifdef TRICORE_TARGET
    assert_target_pointer_alignment(first);
    assert_target_pointer_alignment(second);
    assert_target_pointer_alignment(third);
#else
    assert_host_pointer_alignment(first);
    assert_host_pointer_alignment(second);
    assert_host_pointer_alignment(third);

    *(long double *)second = 1.0L;
    *(long double _Complex *)third = 1.0L + (2.0L * I);
#endif

    memory_free(first);
    memory_free(second);
    memory_free(third);

    return 0;
}
