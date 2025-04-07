#include "alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */
#define MAGIC_NUMBER 0x01234567

static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */

/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size) {
    // Ensure the block is large enough to split
    if (block->size < size + sizeof(free_block)) {
        return NULL; // Not enough space to split
    }

    // Calculate the address for the new block
    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *)split_pnt;

    // Update the new block's properties
    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    // Update the original block's properties
    block->size = size;
    block->next = new_block;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block) {
    free_block *curr = HEAD;
    while(curr != NULL) {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if(next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block) {
    char *block_end = (char*)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while(curr != NULL) {
        if((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block) {
    free_block *curr = HEAD;
    if(curr == block) {
        HEAD = block->next;
        return;
    }
    while(curr != NULL) {
        if(curr->next == block) {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block) {
    if (block == NULL) {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL) {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block) {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block) {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL) {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next) {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
void *do_alloc(size_t size) {
    void *ptr = sbrk(size + sizeof(free_block) + sizeof(int));
    if (ptr == (void *)-1) {
        return NULL; 
    free_block *block = (free_block *)ptr;
    block->size = size;
    block->next = NULL;
    return block;
}


/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
void *tumalloc(size_t size) {
    if (HEAD == NULL) {
        free_block *new_block = (free_block *)do_alloc(size);
        if (new_block == NULL) {
            fprintf(stderr, "Allocation failed in do_alloc\n");
            return NULL; // Allocation failed
        }

        // Write MAGIC_NUMBER
        *(int *)((char *)new_block + sizeof(free_block) + size) = MAGIC_NUMBER;

        return (char *)new_block + sizeof(free_block);
    }

    free_block *curr = HEAD;
    free_block *prev = NULL;

    while (curr != NULL) {
        if (curr->size >= size) {
            if (curr->size < size + sizeof(free_block)) {
                prev = curr;
                curr = curr->next;
                continue;
            }
    
            free_block *header = (free_block *)split(curr, size);
    
            if (header == NULL) {
                fprintf(stderr, "Split failed in tumalloc\n");
                return NULL;
            }
    
            if (prev != NULL) {
                prev->next = curr->next;
            } else {
                HEAD = curr->next;
            }
    
            header->size = size;
            *(int *)((char *)header + sizeof(free_block) + size) = MAGIC_NUMBER;
    
            return (char *)header + sizeof(free_block);
        }
    
        prev = curr;
        curr = curr->next;
    }


    free_block *new_block = (free_block *)do_alloc(size);
    if (new_block == NULL) {
        fprintf(stderr, "Allocation failed in do_alloc\n");
        return NULL; // Allocation failed
    }

    // Write MAGIC_NUMBER
    *(int *)((char *)new_block + sizeof(free_block) + size) = MAGIC_NUMBER;

    return (char *)new_block + sizeof(free_block);
}


/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = tumalloc(total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */
void *turealloc(void *ptr, size_t new_size) {
    if (ptr == NULL) {
        return tumalloc(new_size);
    }
    free_block *header = (free_block *)((char *)ptr - sizeof(free_block));
    if (header->size >= new_size) {
        return ptr; 
    }
    void *new_ptr = tumalloc(new_size);
    if (new_ptr == NULL) {
        return NULL; 
    }
    memcpy(new_ptr, ptr, header->size);
    tufree(ptr);
    return new_ptr;
}


/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    free_block *header = (free_block *)((char *)ptr - sizeof(free_block));

    
    if (*(int *)((char *)header + sizeof(free_block) + header->size) == MAGIC_NUMBER) {
        free_block *free_block_ptr = header;
        free_block_ptr->size = header->size;
        free_block_ptr->next = HEAD;
        HEAD = free_block_ptr;

        coalesce(free_block_ptr);
    } else {
        fprintf(stderr, "MEMORY CORRUPTION DETECTED\n");
        abort();
    }
}