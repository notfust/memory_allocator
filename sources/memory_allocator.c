/**
 * \file memory_allocator.c
 * \date 14.01.2026
 * \brief Implementation of memory allocator for embedded systems
 */

#include "../includes/memory_allocator.h"



/** \brief Size of memory heap in bytes
 * \private
 */
#define HEAP_SIZE       (2048)

/** \brief Minimum usable payload size for a new block
 * \private
 */
#define MIN_USEFUL_SIZE (MEMORY_ALLOCATOR_ALIGNMENT)

/** \brief Magic number for block validation
 * \private
 */
#define BLOCK_MAGIC     (0xDEADBEEF)



/** \brief Structure for the memory block header
 * \private
 */
typedef struct memory_block {
    uint32_t             magic;   /**< \brief Magic number for validation */
    size_t               size;    /**< \brief Payload size in bytes */
    uint8_t              is_free; /**< \brief Flag: 1 - free, 0 - occupied */
    struct memory_block *next;    /**< \brief Pointer to the next block */
    struct memory_block *prev;    /**< \brief Pointer to the previous block */
} memory_block_t;


/** \brief C99-compatible storage for an aligned heap region
 * \private
 *
 * Every union member starts at the union address.  The byte region is thus
 * aligned for all standard scalar types represented by
 * memory_allocator_alignment_t, including the alignment required by the
 * platform-specific payload contract.
 */
typedef union memory_heap_storage {
    memory_allocator_alignment_t alignment;
    uint8_t                      bytes[HEAP_SIZE];
} memory_heap_storage_t;


/** \brief Memory heap
 * \private
 */
static memory_heap_storage_t heap_storage = { { 0 } };

/** \brief A pointer to the first block in the heap
 * \private
 */
static memory_block_t *first_block = NULL;



/** \brief Align size to the nearest multiple of `align` bytes for better performance
 *
 * \param size Size to align
 * \param align Alignment in bytes
 * \return Aligned size
 * \private
 */
static size_t align_size(size_t size, size_t align)
{
    size_t remainder = size % align;

    if (remainder == 0U) { return size; }

    return size + (align - remainder);
}

/** \brief Get the padded size of a block header
 *
 * Padding the header ensures that the first payload byte remains aligned even
 * when sizeof(memory_block_t) is not a multiple of the allocator alignment.
 *
 * \return Header size including trailing alignment padding
 * \private
 */
static size_t block_header_size(void)
{
    return align_size(sizeof(memory_block_t), MEMORY_ALLOCATOR_ALIGNMENT);
}

/** \brief Calculate the total size of a memory block including header
 *
 * \param data_size Size of user data in bytes
 * \return Total block size including header
 * \private
 */
static size_t block_total_size(size_t data_size)
{
    return block_header_size() + data_size;
}

/**
 * \brief Get pointer to user data area from block header
 *
 * \param block Memory block header
 * \return Pointer to user data
 * \private
 */
static void *block_data_ptr(memory_block_t *block)
{
    return (void *)((uint8_t *)block + block_header_size());
}

/** \brief Merge adjacent free blocks
 * \private
 */
static void memory_merge_free_blocks(void)
{
    memory_block_t *current = first_block;

    while (current != NULL && current->next != NULL) {
        // Merge with next block if both are free
        if (current->is_free && current->next->is_free) {
            current->size += block_total_size(current->next->size);
            current->next  = current->next->next;

            if (current->next != NULL) { current->next->prev = current; }

        } else {
            current = current->next;
        }
    }
}

/** \brief Validate a memory block
 *
 * \param block Pointer to the memory block to validate
 * \return TRUE if the block is valid, FALSE otherwise
 * \private
 */
static bool is_valid_block(memory_block_t *block)
{
    // Checking for a null pointer
    if (block == NULL) { return FALSE; }

    // Checking that a block is inside our heap
    if ((uint8_t *)block < heap_storage.bytes
        || (uint8_t *)block + block_header_size() > (heap_storage.bytes + HEAP_SIZE)) {
        return FALSE;
    }

    // Checking the magic number
    if (block->magic != BLOCK_MAGIC) { return FALSE; }

    // Checking that the block size is reasonable
    if (block->size > HEAP_SIZE) { return FALSE; }

    return TRUE;
}



void memory_init(void)
{
    // Creating the first block that occupies the entire heap
    first_block          = (memory_block_t *)heap_storage.bytes;
    first_block->magic   = BLOCK_MAGIC;
    first_block->size    = HEAP_SIZE - block_header_size();
    first_block->is_free = TRUE;
    first_block->next    = NULL;
    first_block->prev    = NULL;
}

void *memory_alloc(size_t size)
{
    if (size == 0 || first_block == NULL) { return NULL; }

    memory_block_t *current = first_block;

    // Align size for better performance
    size = align_size(size, MEMORY_ALLOCATOR_ALIGNMENT);

    // We are looking for the first suitable free block (First-Fit algorithm)
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // If the block is too big, we split it
            if (current->size > size + block_total_size(MIN_USEFUL_SIZE)) {
                memory_block_t *next_block = (memory_block_t *)((uint8_t *)current + block_total_size(size));
                next_block->magic          = BLOCK_MAGIC;
                next_block->size           = current->size - block_total_size(size);
                next_block->is_free        = TRUE;
                next_block->next           = current->next;
                next_block->prev           = current;

                if (current->next != NULL) { current->next->prev = next_block; }

                current->size    = size;
                current->is_free = FALSE;
                current->next    = next_block;

            } else {
                // We'll use the entire block
                current->is_free = FALSE;
            }

            return block_data_ptr(current);
        }

        current = current->next;
    }

    // Still no memory available
    return NULL;
}

void memory_free(void *ptr)
{
    // We get a pointer to the block header
    memory_block_t *block = (memory_block_t *)((uint8_t *)ptr - block_header_size());

    if (!is_valid_block(block)) { return; }

    block->is_free = TRUE;

    // Coalesce with previous blocks if free
    if (block->prev != NULL && block->prev->is_free) {
        block->prev->size += block_total_size(block->size);
        block->prev->next  = block->next;

        if (block->next != NULL) { block->next->prev = block->prev; }

        block = block->prev;
    }

    // Coalesce with next blocks if free
    if (block->next != NULL && block->next->is_free) {
        block->size += block_total_size(block->next->size);
        block->next  = block->next->next;

        if (block->next != NULL) { block->next->prev = block; }
    }
}
