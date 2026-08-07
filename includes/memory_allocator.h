/**
 * \file memory_allocator.h
 * \date 14.01.2026
 * \brief Memory allocator for embedded systems
 *
 * Usage example:
 * \code{c}
 * int main(void) {
 *     memory_init();
 *
 *     // Allocate memory for different objects
 *     int* array = (int*)memory_alloc(10 * sizeof(int));
 *     char* buffer = (char*)memory_alloc(64);
 *     float* sensor_data = (float*)memory_alloc(4 * sizeof(float));
 *
 *     // Use memory
 *     if (array) {
 *         for (int i = 0; i < 10; i++) {
 *             array[i] = i * 2;
 *         }
 *     }
 *
 *     // Free memory
 *     memory_free(array);
 *     memory_free(sensor_data);
 *
 *     // After freeing, we can allocate memory again
 *     void* new_alloc = memory_alloc(128);
 *
 *     return 0;
 * }
 * \endcode
 */
#ifndef MEMORY_ALLOCATOR_H_
#define MEMORY_ALLOCATOR_H_


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 * \brief Type used to obtain the allocator's C99 payload alignment.
 *
 * A union is aligned to satisfy each of its members.  It is used only to
 * align allocator-owned storage without C11's `_Alignas` or
 * compiler-specific attributes.
 *
 * The allocator guarantees this alignment for every returned pointer.  It is
 * sufficient for the standard scalar types represented below and for
 * structures and unions composed from them.  Platform-specific types with a
 * stricter requirement must not be stored in allocator memory until the
 * platform layer extends this type and validates the resulting contract.
 */
typedef union memory_allocator_alignment {
    void *object_pointer;
    void (*function_pointer)(void);
    wchar_t wide_character;
    signed char signed_character;
    unsigned char unsigned_character;
    short signed_short;
    unsigned short unsigned_short;
    int signed_integer;
    unsigned int unsigned_integer;
    long signed_long;
    unsigned long unsigned_long;
    long long signed_long_long;
    unsigned long long unsigned_long_long;
    float single_precision;
    double double_precision;
    long double extended_precision;
    float _Complex single_precision_complex;
    double _Complex double_precision_complex;
    long double _Complex extended_precision_complex;
} memory_allocator_alignment_t;

/**
 * \brief Guaranteed alignment, in bytes, of memory_alloc() results.
 *
 * For a TriCore EABI target, standard scalar types require at most four-byte
 * alignment, so the target contract is fixed at four bytes.  Other targets
 * use the alignment wrapper size as a portable C99 upper bound for the
 * standard scalar types supported by their active toolchain.
 *
 * The storage wrapper and this placement quantum have distinct purposes:
 * the wrapper aligns the heap base, while this macro aligns every header and
 * payload within that heap.
 */
#ifdef TRICORE_TARGET
#define MEMORY_ALLOCATOR_ALIGNMENT ((size_t)4U)
#else
#define MEMORY_ALLOCATOR_ALIGNMENT ((size_t)sizeof(memory_allocator_alignment_t))
#endif


// Boolean definitions
#ifndef TRUE
#define TRUE (1)
#endif /* TRUE */

#ifndef FALSE
#define FALSE (0)
#endif /* FALSE */



/** \brief Initialize the memory allocator */
void memory_init(void);

/** \brief Allocate memory from the heap
 *
 * \param size Required size in bytes
 * \return Pointer to allocated memory or NULL in case of error
 */
void* memory_alloc(size_t size);

/** \brief Free allocated memory
 *
 * \param ptr Pointer to the memory to be freed
 * \return Error code indicating the result of the operation
 */
void memory_free(void* ptr);


#endif /* MEMORY_ALLOCATOR_H_ */
