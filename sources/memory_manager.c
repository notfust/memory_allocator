/**
 * \file memory_manager.c
 * \date 14.01.2026
 * \brief Implementation of memory manager for embedded systems
 */

#include "../includes/memory_manager.h"



/** \brief Size of memory heap in bytes
 * \private
 */
#define HEAP_SIZE (2048)



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
static uint8_t heap[HEAP_SIZE]     = { 0 };

/** \brief A pointer to the first block in the heap
 * \private
 */
static memory_block_t *first_block = NULL;

