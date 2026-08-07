#include <assert.h>
#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#include "../includes/memory_allocator.h"
#include "../includes/memory_allocator_platform.h"

#define EXHAUSTION_POINTER_CAPACITY (256U)
#define SERIES_ALLOCATION_COUNT     (12U)

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
    char prefix;
    long double _Complex value;
} complex_alignment_probe_t;

#define POINTER_ALIGNMENT     offsetof(pointer_alignment_probe_t, value)
#define LONG_LONG_ALIGNMENT   offsetof(long_long_alignment_probe_t, value)
#define LONG_DOUBLE_ALIGNMENT offsetof(long_double_alignment_probe_t, value)
#define COMPLEX_ALIGNMENT     offsetof(complex_alignment_probe_t, value)

static void assert_allocator_alignment(const void *pointer)
{
    assert((((uintptr_t)pointer) % MEMORY_ALLOCATOR_ALIGNMENT) == 0U);

#ifndef TRICORE_TARGET
    assert((((uintptr_t)pointer) % POINTER_ALIGNMENT) == 0U);
    assert((((uintptr_t)pointer) % LONG_LONG_ALIGNMENT) == 0U);
    assert((((uintptr_t)pointer) % LONG_DOUBLE_ALIGNMENT) == 0U);
    assert((((uintptr_t)pointer) % COMPLEX_ALIGNMENT) == 0U);
#endif
}

static void test_minimum_allocation_and_split(void)
{
    void *first;
    void *second;

    memory_init();

    first  = memory_alloc(1U);
    second = memory_alloc(1U);

    assert(first != NULL);
    assert(second != NULL);
    assert(first != second);
    assert_allocator_alignment(first);
    assert_allocator_alignment(second);

    memory_free(first);
    memory_free(second);
}

static void test_heap_exhaustion(void)
{
    void  *allocations[EXHAUSTION_POINTER_CAPACITY];
    size_t allocation_count = 0U;
    void  *allocation;

    memory_init();

    while (allocation_count < EXHAUSTION_POINTER_CAPACITY) {
        allocation = memory_alloc(1U);
        if (allocation == NULL) { break; }

        assert_allocator_alignment(allocation);
        allocations[allocation_count] = allocation;
        ++allocation_count;
    }

    assert(allocation_count > 0U);
    assert(allocation_count < EXHAUSTION_POINTER_CAPACITY);
    assert(memory_alloc(1U) == NULL);

    while (allocation_count > 0U) {
        --allocation_count;
        memory_free(allocations[allocation_count]);
    }
}

static void test_alignment_after_split_and_coalesce(void)
{
    void  *allocations[SERIES_ALLOCATION_COUNT];
    size_t index;
    void  *large_allocation;

    memory_init();

    for (index = 0U; index < SERIES_ALLOCATION_COUNT; ++index) {
        allocations[index] = memory_alloc(index + 1U);
        assert(allocations[index] != NULL);
        assert_allocator_alignment(allocations[index]);
    }

    for (index = 0U; index < SERIES_ALLOCATION_COUNT; index += 2U) { memory_free(allocations[index]); }

    for (index = 1U; index < SERIES_ALLOCATION_COUNT; index += 2U) { memory_free(allocations[index]); }

    large_allocation = memory_alloc(1900U);
    assert(large_allocation != NULL);
    assert_allocator_alignment(large_allocation);
    memory_free(large_allocation);
}

int main(void)
{
#ifdef TRICORE_TARGET
    assert(MEMORY_ALLOCATOR_ALIGNMENT == 4U);
#else
    assert((MEMORY_ALLOCATOR_ALIGNMENT % POINTER_ALIGNMENT) == 0U);
    assert((MEMORY_ALLOCATOR_ALIGNMENT % LONG_LONG_ALIGNMENT) == 0U);
    assert((MEMORY_ALLOCATOR_ALIGNMENT % LONG_DOUBLE_ALIGNMENT) == 0U);
    assert((MEMORY_ALLOCATOR_ALIGNMENT % COMPLEX_ALIGNMENT) == 0U);
#endif

    test_minimum_allocation_and_split();
    test_heap_exhaustion();
    test_alignment_after_split_and_coalesce();

#ifndef TRICORE_TARGET
    {
        long double          *real_value;
        long double _Complex *complex_value;

        memory_init();
        real_value    = (long double *)memory_alloc(sizeof(*real_value));
        complex_value = (long double _Complex *)memory_alloc(sizeof(*complex_value));
        assert(real_value != NULL);
        assert(complex_value != NULL);
        *real_value    = 1.0L;
        *complex_value = 1.0L + (2.0L * I);
        memory_free(real_value);
        memory_free(complex_value);
    }
#endif

    return 0;
}
