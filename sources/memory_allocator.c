/**
 * \file memory_allocator.c
 * \date 14.01.2026
 * \brief Implementation of memory allocator for embedded systems
 */

#include "../includes/memory_allocator.h"

#include "../includes/memory_allocator_platform.h"


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


/** \brief Structure copied to and from each block header
 * \private
 */
typedef struct memory_block {
    uint32_t magic;   /**< \brief Magic number for validation */
    size_t   size;    /**< \brief Payload size in bytes */
    uint8_t  is_free; /**< \brief Flag: 1 - free, 0 - occupied */
    uint8_t *next;    /**< \brief Address of the next block header */
    uint8_t *prev;    /**< \brief Address of the previous block header */
} memory_block_t;

/** \brief C99-compatible storage for an aligned heap region
 * \private
 *
 * The alignment member makes bytes[0] suitably aligned for the platform
 * contract. Metadata is accessed only by byte copies, never by treating the
 * byte array itself as a memory_block_t object.
 */
typedef union memory_heap_storage {
    memory_allocator_storage_alignment_t alignment;
    uint8_t                              bytes[HEAP_SIZE];
} memory_heap_storage_t;


/** \brief Memory heap
 * \private
 */
static memory_heap_storage_t heap_storage = { { 0 } };

/** \brief Address of the first block header in the heap
 * \private
 */
static uint8_t *first_block = NULL;


/** \brief Copy bytes without depending on libc memcpy()
 *
 * Source and destination must not overlap.
 *
 * \param destination Destination byte range
 * \param source Source byte range
 * \param size Number of bytes to copy
 * \private
 */
static void copy_bytes(void *destination, const void *source, size_t size)
{
    uint8_t       *destination_bytes = (uint8_t *)destination;
    const uint8_t *source_bytes      = (const uint8_t *)source;

    for (size_t index = 0; index < size; ++index) { destination_bytes[index] = source_bytes[index]; }
}

/** \brief Read a block header from byte storage
 * \private
 */
static void block_load(const uint8_t *address, memory_block_t *block)
{ 
    copy_bytes(block, address, sizeof(*block)); 
}

/** \brief Write a block header to byte storage
 * \private
 */
static void block_store(uint8_t *address, const memory_block_t *block)
{ 
    copy_bytes(address, block, sizeof(*block)); 
}

/** \brief Add two sizes with overflow detection
 *
 * \param left First operand
 * \param right Second operand
 * \param result Destination for the sum
 * \return true if the sum fits in size_t
 * \private
 */
static bool add_sizes(size_t left, size_t right, size_t *result)
{
    if (result == NULL || left > (SIZE_MAX - right)) { return false; }

    *result = left + right;

    return true;
}

/** \brief Align a size to the allocator alignment contract
 *
 * \param size Size to align
 * \param aligned_size Destination for the aligned result
 * \return true if alignment succeeds without size_t overflow
 * \private
 */
static bool align_allocator_size(size_t size, size_t *aligned_size)
{
    size_t remainder;

    if (aligned_size == NULL) { return false; }

    remainder = size % MEMORY_ALLOCATOR_ALIGNMENT;
    if (remainder == 0U) {
        *aligned_size = size;
        return true;
    }

    return add_sizes(size, MEMORY_ALLOCATOR_ALIGNMENT - remainder, aligned_size);
}

/** \brief Get the padded size of a block header
 *
 * Padding the header ensures that the first payload byte remains aligned even
 * when sizeof(memory_block_t) is not a multiple of the allocator alignment.
 *
 * \param header_size Destination for header size including trailing padding
 * \return true if the header size fits in size_t
 * \private
 */
static bool block_header_size(size_t *header_size)
{
    return align_allocator_size(sizeof(memory_block_t), header_size);
}

/** \brief Calculate the total size of a memory block including header
 *
 * \param data_size Size of user data in bytes
 * \param total_size Destination for total block size
 * \return true if the total size fits in size_t
 * \private
 */
static bool block_total_size(size_t data_size, size_t *total_size)
{
    size_t header_size;

    if (!block_header_size(&header_size)) { return false; }

    return add_sizes(header_size, data_size, total_size);
}

/** \brief Get pointer to user data area from a block header address
 * \private
 */
static void *block_data_ptr(uint8_t *block_address)
{
    size_t header_size;

    if (!block_header_size(&header_size)) { return NULL; }

    return (void *)(block_address + header_size);
}

/** \brief Validate and read a memory block header
 *
 * \param address Address of the candidate block header
 * \param block Destination for the copied header
 * \return true if the header belongs to the heap and has valid magic
 * \private
 */
static bool is_valid_block(const uint8_t *address, memory_block_t *block)
{
    size_t header_size;

    if (address == NULL || block == NULL) { return FALSE; }

    if (!block_header_size(&header_size)) { return FALSE; }

    if (address < heap_storage.bytes || address + header_size > heap_storage.bytes + HEAP_SIZE) { return FALSE; }

    block_load(address, block);

    if (block->magic != BLOCK_MAGIC) { return FALSE; }

    if (block->size > HEAP_SIZE) { return FALSE; }

    return TRUE;
}

void memory_init(void)
{
    memory_block_t first = { 0 };
    size_t         header_size;

    if (!block_header_size(&header_size) || header_size > HEAP_SIZE) {
        first_block = NULL;
        return;
    }

    first.magic   = BLOCK_MAGIC;
    first.size    = HEAP_SIZE - header_size;
    first.is_free = TRUE;
    first.next    = NULL;
    first.prev    = NULL;

    first_block = heap_storage.bytes;
    block_store(first_block, &first);
}

void *memory_alloc(size_t size)
{
    uint8_t *current_address;
    size_t   minimum_block_size;

    if (size == 0 || first_block == NULL) { return NULL; }

    if (!align_allocator_size(size, &size)) { return NULL; }

    if (!block_total_size(MIN_USEFUL_SIZE, &minimum_block_size)) { return NULL; }

    current_address = first_block;

    while (current_address != NULL) {
        memory_block_t current;

        block_load(current_address, &current);

        if (current.is_free && current.size >= size) {
            size_t remaining_size = current.size - size;

            if (remaining_size > minimum_block_size) {
                size_t         allocated_block_size;
                uint8_t       *next_address;
                memory_block_t next         = { 0 };

                if (!block_total_size(size, &allocated_block_size)) { return NULL; }

                next_address = current_address + allocated_block_size;

                next.magic   = BLOCK_MAGIC;
                next.size    = current.size - allocated_block_size;
                next.is_free = TRUE;
                next.next    = current.next;
                next.prev    = current_address;

                if (current.next != NULL) {
                    memory_block_t following;

                    block_load(current.next, &following);
                    following.prev = next_address;
                    block_store(current.next, &following);
                }

                current.size    = size;
                current.is_free = FALSE;
                current.next    = next_address;

                block_store(next_address, &next);
                block_store(current_address, &current);
            } else {
                current.is_free = FALSE;
                block_store(current_address, &current);
            }

            return block_data_ptr(current_address);
        }

        current_address = current.next;
    }

    return NULL;
}

void memory_free(void *ptr)
{
    uint8_t       *block_address;
    memory_block_t block;
    size_t         header_size;
    bool           neighbours_changed = false;

    if (ptr == NULL) { return; }

    if (!block_header_size(&header_size)) { return; }

    block_address = (uint8_t *)ptr - header_size;

    if (!is_valid_block(block_address, &block)) { return; }

    block.is_free = TRUE;

    if (block.prev != NULL) {
        memory_block_t previous;

        if (is_valid_block(block.prev, &previous) && previous.is_free) {
            size_t block_size_with_header;
            size_t merged_size;
            uint8_t *next_address = block.next;

            if (!block_total_size(block.size, &block_size_with_header) || !add_sizes(previous.size, block_size_with_header, &merged_size)) { return; }

            block_address = block.prev;
            block         = previous;
            block.size    = merged_size;
            block.is_free = TRUE;
            block.next    = next_address;
            neighbours_changed = true;
        }
    }

    if (block.next != NULL) {
        memory_block_t next;

        if (is_valid_block(block.next, &next) && next.is_free) {
            size_t next_size_with_header;
            size_t merged_size;

            if (!block_total_size(next.size, &next_size_with_header) || !add_sizes(block.size, next_size_with_header, &merged_size)) { return; }

            block.size = merged_size;
            block.next = next.next;
            neighbours_changed = true;
        }
    }

    if (neighbours_changed && block.next != NULL) {
        memory_block_t following;

        block_load(block.next, &following);
        following.prev = block_address;
        block_store(block.next, &following);
    }

    block_store(block_address, &block);
}
