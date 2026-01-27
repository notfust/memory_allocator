/**
 * \file memory_manager.c
 * \date 14.01.2026
 * \brief Implementation of memory manager for embedded systems
 */

#include "../includes/memory_manager.h"



/** \brief Size of memory heap in bytes
 * \private
 */
#define HEAP_SIZE       (2048)

/** \brief Minimum usable size for a new block
 * \private
 */
#define MIN_USEFUL_SIZE (8)



/** \brief Structure for the memory block header
 * \private
 */
typedef struct memory_block {
    size_t               size;    /**< \brief Block size (including header) */
    uint8_t              is_free; /**< \brief Flag: 1 - free, 0 - occupied */
    struct memory_block *next;    /**< \brief Pointer to the next block */
    struct memory_block *prev;    /**< \brief Pointer to the previous block */
} memory_block_t;



/** \brief Memory heap
 * \private
 */
static uint8_t heap[HEAP_SIZE] = { 0 };

/** \brief A pointer to the first block in the heap
 * \private
 */
static memory_block_t *first_block = NULL;



void memory_init(void)
{
    // Creating the first block that occupies the entire heap
    first_block          = (memory_block_t *)heap;
    first_block->size    = HEAP_SIZE - sizeof(memory_block_t);
    first_block->is_free = TRUE;
    first_block->next    = NULL;
    first_block->prev    = NULL;
}

static void *memory_alloc(size_t size)
{
    if (size == 0 || first_block == NULL) { return NULL; }

    memory_block_t *current = first_block;

    // We are looking for the first suitable free block (First-Fit algorithm)
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // If the block is too big, we split it
            if (current->size > size + sizeof(memory_block_t) + MIN_USEFUL_SIZE) {
                memory_block_t *next_block = (memory_block_t *)((uint8_t *)current + sizeof(memory_block_t) + size);
                next_block->size           = current->size - (sizeof(memory_block_t) + size);
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

            return (void *)((uint8_t *)current + sizeof(memory_block_t));
        }

        current = current->next;
    }

    // Still no memory available
    return NULL;
}

