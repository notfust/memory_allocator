/**
 * \file memory_allocator_platform.h
 * \brief Platform-specific configuration for the memory allocator
 */
#ifndef MEMORY_ALLOCATOR_PLATFORM_H_
#define MEMORY_ALLOCATOR_PLATFORM_H_

#include <stddef.h>

/**
 * \brief C99 wrapper used to align allocator-owned static storage.
 *
 * This type is private to the allocator implementation.  It makes the base
 * address of the static byte region suitable for each listed scalar type
 * without C11 `_Alignas` or compiler-specific attributes.
 */
typedef union memory_allocator_storage_alignment {
    void              *object_pointer;
    void               (*function_pointer)(void);
    wchar_t            wide_character;
    signed char        signed_character;
    unsigned char      unsigned_character;
    short              signed_short;
    unsigned short     unsigned_short;
    int                signed_integer;
    unsigned int       unsigned_integer;
    long               signed_long;
    unsigned long      unsigned_long;
    long long          signed_long_long;
    unsigned long long unsigned_long_long;
    float              single_precision;
    double             double_precision;
    long double        extended_precision;
    float _Complex single_precision_complex;
    double _Complex double_precision_complex;
    long double _Complex extended_precision_complex;
} memory_allocator_storage_alignment_t;

/**
 * \brief Alignment contract for allocator headers and payload addresses.
 *
 * The TriCore configuration follows the four-byte scalar alignment contract
 * used by this allocator. The host fallback uses the offset of an alignment
 * wrapper after a char field. This C99 constant is a safe placement quantum
 * for every member of memory_allocator_storage_alignment_t; unlike sizeof a
 * union, it does not over-align merely because a member has a large size.
 *
 * Define TRICORE_TARGET only in the target build configuration.
 */
#ifdef TRICORE_TARGET
#define MEMORY_ALLOCATOR_ALIGNMENT ((size_t)4U)
#else
typedef struct memory_allocator_storage_alignment_probe {
    char                                 prefix;
    memory_allocator_storage_alignment_t value;
} memory_allocator_storage_alignment_probe_t;

#define MEMORY_ALLOCATOR_ALIGNMENT ((size_t)offsetof(memory_allocator_storage_alignment_probe_t, value))
#endif

#endif /* MEMORY_ALLOCATOR_PLATFORM_H_ */
