#include "allocator.h"
#include "debug_break.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 
  IMPLEMENTATION 
  This allocator uses an implicit free list design where each block contains
  a header with size information and allocation status. The heap is managed
  as a single contiguous region with blocks laid out sequentially, and the 
  heap uses a first-fit traversal allocation strategy. You can find more detailed 
  information of the implementation on the readme file. 
 
  Block Format:
  - Header (8 bytes): Contains payload size and allocation bit (LSB)
  - Payload: User data space
 */


// Global heap management variables 
static void *heapStart; // Pointer to the beginning of heap region 
static size_t heapSize; // Total size of heap in bytes 
static size_t sizeUsed; // Total bytes currently allocated 

// This function rounds up the size to the nearest mutliple of eight
size_t roundup(size_t number) {
    return (number + 8 - 1) & ~(8 - 1);
}

// This function splits up a block if it's significantly larger than the requested size. 
void splitFunc(size_t *used, size_t *h, size_t payload, size_t requested_size) {
    /* - Creates a new free block from the remaining space. 
       - Split only occurs if the remainder is at least 16 bytes (8-bytes for the 
         header + 8-bytes required for the minimum payload) 
    */ 

    if ((payload - requested_size) >= 16) {
        // Calculates the address for the new block header 
        unsigned char *split_address = (unsigned char *)h + (8 + requested_size); 
        size_t *split;
        split = (size_t *)split_address; 

        // Sets up new free block with the remaining space 
        *split = payload - (requested_size + 8); 

        // Updates original block to the requested size 
        *h = requested_size; // size requested by user 
        *used = (8 + requested_size); // output of actual bytes used during allocation 
    }
} 

// This function initializes the heap allocator with the given memory region 
bool myinit(void *heap_start, size_t heap_size) {
    if (heap_start == NULL) {
        return false;
    }
    
    // Initializes global heap state
    heapStart = heap_start; // Pointer to the start of the heap memory 
    heapSize = heap_size; // Total size of the heap in bytes 

    // Creates initial free block header spanning the entire heap 
    size_t *header = (size_t *)heapStart;
    *header = heapSize - 8; // payload size which is equal to the total size - header size
    sizeUsed = 0;
    return true; // returns true if the initialization is successful and false otherwise 
}

// This function allocates a memory block of the requested size 
void *mymalloc(size_t requested_size) {
    if (requested_size == 0) {
        return NULL;
    }
    
    // Rounds up for alignment and check size limits 
    requested_size = roundup(requested_size);
    if (requested_size > MAX_REQUEST_SIZE || (requested_size + sizeUsed) > heapSize) {
        return NULL;
    }
    
    size_t *header;
    size_t payload;
    size_t state;
    
    // Traverses heap and find large enough block
    for (size_t i = 0; i < heapSize; i += 8) {
        unsigned char *newIndex = (unsigned char *)heapStart + i;
        header = (size_t *)newIndex;
        payload = *header;
        state = *header & 1; // extracts the allocation bit 
        
        if (state == 0) {
            // Checks if the payload is large enough for the requested size
            if (requested_size <= MAX_REQUEST_SIZE && requested_size <= payload) {
                size_t used;
                used = 8 + payload;

                // Checks block and split if significantly larger than needed 
                if ((payload - requested_size) >= 16) {
                    splitFunc(&used, header, payload, requested_size);
                }

                // Marks block as allocated 
                *header ^= 1;
                sizeUsed += used; 

                // Returns pointer to payload (skip header) 
                unsigned char *payload_address = (unsigned char *)header + 8;
                void *ptr = payload_address;
                return ptr; // returns pointer to the allocated payload 
            }
        }
        
        // Allocated block - clear allocation bit (to get the size)
        if (state == 1) {
            payload ^= 1;
        }   
        
        // Moves to the next block 
        i += payload;
    }
    return NULL; // returns NULL if allocation failed 
}

// This function frees the previously allocated memory blocks by clearing allocation bit in header 
void myfree(void *ptr) {
    if (ptr != NULL) { 
        // Finds header by going back 8 bytes from payload 
        unsigned char *h = (unsigned char *)ptr - 8;
        size_t *header = (size_t *)h; 

        // Clears allocation bit to mark it as free 
        *header ^= 1; 

        // Updates the global usage counter 
        sizeUsed -= (*header + 8);
    }
}

// This function reallocates the memory block to the new size 
void *myrealloc(void *old_ptr, size_t new_size) {
    /* Uses a simple approach 
        - Find new block 
        - Copy data 
        - Free old block  
    */ 

    // Handles free case 
    if (new_size == 0 && old_ptr != NULL) {
        myfree(old_ptr);
        return old_ptr;
    }
    
    // Checks the size limits 
    size_t requested_size = roundup(new_size);
    if (requested_size > MAX_REQUEST_SIZE || (requested_size + sizeUsed) > heapSize) {
        return NULL;
    }

    // Handles malloc case 
    if (old_ptr == NULL) {
        return mymalloc(new_size);
    }
    
    size_t *header;
    size_t payload;
    size_t state;
    
    // Traverses heap to find large enough block using first-fit strategy 
    for (size_t i = 0; i < heapSize; i += 8) {
        unsigned char *newindex = (unsigned char *)heapStart + i;
        header = (size_t *)newindex;
        payload = *header;
        state = *header & 1;
        
        if (state == 0) { // free block 
            // Checks if the payload is large enough for the requested size
            if (requested_size <= MAX_REQUEST_SIZE && requested_size <= payload) {
                size_t used = 8 + payload;

                // Gets old block info 
                unsigned char *old_header = (unsigned char *)old_ptr - 8;
                size_t *old_h = (size_t *)old_header;
                
                // If current block is large enough, return without reallocating 
                if ((*old_h ^ 1) > new_size) {
                    return old_ptr; 
                } 

                // Frees the old block 
                *old_h ^= 1;
                sizeUsed -= (*old_h + 8);
                
                // Splits the new block if necessary 
                if ((payload - requested_size) >= 16) {
                    splitFunc(&used, header, payload, requested_size);
                }
                
                // Allocates the new block 
                *header ^= 1;
                sizeUsed += used; 

                // Copies the data from the old to the new location 
                unsigned char *payload_address = (unsigned char *)header + 8;
                void *ptr = payload_address;
                memmove(ptr, old_ptr, *old_h); // Uses size of old block for the copy 
                return ptr;
            }
        }

        if (state == 1) { // Skips allocated blocks 
            payload ^= 1;
        }
        
        i += payload;
    }
    return NULL;
}

// Validates the heap consistency by checking the internal data structures 
bool validate_heap() {
    /* Verifies that: 
        - Blocks account for the total heap size 
        - Global usage counter matches the actual allocated space 
        - There is no corruption in block headers 
     */
    
    // Basic check 
    if (sizeUsed > heapSize) {
        return false;
    } 

    size_t *h;
    size_t used = 0; // Running total of the allocated space 
    size_t freed = 0; // Running total of the free space 

    // Traverses through the heap and tallies the freed and used space
    for (size_t i = 0; i < heapSize; i += 8) {
        unsigned char *current_index = (unsigned char *)heapStart + i;
        h = (size_t *)current_index;
        size_t payload = *h ^ 1; // removes the allocation bit to get the size 
        size_t state = *h & 1; // Extracts the allocation bit 
        
        size_t current_used;
        size_t current_free;
        if (state == 0) { // free block 
            payload = *h; 
            current_free = 8 + payload;
            freed += current_free;
        } else if (state == 1) { // allocated block 
            current_used = 8 + payload;
            used += current_used;
        } 

        i += payload; // Move to the next block 
    }

    // Check the housekeeping information about the parameters
    if ((freed + used) != heapSize) {
        breakpoint(); // breakpoint for investigation 
        return false;
    } 

    if (sizeUsed != used) { // Global counter mismatch 
        breakpoint(); 
        return false;
    } 

    return true; // returns true if heap is valid and false whenever corruption is detected 
}

// This file dumps the contents of the heap by printing them out
void dump_heap() { 
    // Displays heap boundaries. usage statistics, and block information 
    printf("Heap starts at address %p and ends at %p. %lu bytes currently used.\n", heapStart, (unsigned char *)heapStart + heapSize, sizeUsed);
    
    size_t index = 0;
    while (index < heapSize) {
        unsigned char *curr = (unsigned char *)heapStart + index;
        size_t *cur = (size_t *)curr; 

        printf("The payload is: %zu", *cur);
        index += (*cur + 8); // Moves to the next block 
    }
        
        
}