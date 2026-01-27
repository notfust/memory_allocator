/**
 * \file memory_manager.h
 * \date 14.01.2026
 * \brief Memory manager for embedded systems
 */
#ifndef MEMORY_MANAGER_H_
#define MEMORY_MANAGER_H_

#include <stddef.h>
#include <stdint.h>



// Boolean definitions
#define TRUE  (1)
#define FALSE (0)



/** \brief Initialize the memory manager */
void memory_init(void);

/** \brief Allocate memory from the heap
 *
 * \param size Required size in bytes
 * \return Pointer to allocated memory or NULL in case of error
 */
void* memory_alloc(size_t size);


#endif /* MEMORY_MANAGER_H_ */
