/**
 * \file memory_manager.h
 * \date 14.01.2026
 * \brief Memory manager for embedded systems
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

/** \brief Free allocated memory
 *
 * \param ptr Pointer to the memory to be freed
 * \return Error code indicating the result of the operation
 */
void memory_free(void* ptr);


#endif /* MEMORY_MANAGER_H_ */
