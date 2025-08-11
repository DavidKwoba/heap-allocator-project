#include "allocator.h"
#include "debug_break.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 
  EXPLICIT HEAP IMPLEMENTATION 
  This allocator uses an explicit free list design where free blocks are
  organized in a doubly-linked list for efficient free space management.
  The heap maintains both allocated and free blocks with comprehensive
  header information and implements coalescing for fragmentation reduction. 
  You can find more detailed information of the implementation on the readme file. 
 
  Block Format:
  - Header (24 bytes): Contains payload size, allocation bit (LSB), and
    doubly-linked list pointers (prev/next) for free blocks
  - Payload: User data space (8-byte aligned) 

  Free List Management:
    - Doubly-linked list of free blocks pointed to by freeEnd
    - LIFO insertion strategy (new free blocks added to front)
    - Coalescing with immediate right neighbor during deallocation
 */ 


// Global state variables 
static void *heapStart; // Pointer to the beginning of heap region 
static size_t heapSize; // Total size of heap in bytes 
static size_t sizeUsed; // Total bytes currently being used (includes header)
static void *freeEnd; // Pointer to the head of the first free block 
static size_t freeSpace; // Total bytes available for allocation 

// TYPE DELCARATION FOR STRUCT
typedef struct {
    size_t h; // payload size with status bit 
    size_t *prev; // previous free block 
    size_t *next; // next free block 
} curr_header;

// This function rounds up a number to the nearest multiple of 8 for alignment 
size_t roundup(size_t number) {
    return (number + 8 - 1) & ~(8 - 1); 
}

// This function splits up a free block if it's significantly larger than the requested size. 
void splitFunc(size_t *currentFree, size_t *used, size_t *payload, size_t requested_size) {
    /* - Splits a free block into an allocated block and a remaining free one 
       - Creates a new free block from the remaining space. 
    */     
    
    curr_header split;
    curr_header mystruct = *(curr_header *)currentFree; 

    // Calculates the address where the new free block will start 
    unsigned char *split_address = (unsigned char *)currentFree + 16 + requested_size; 

    // Initializes the header of the new free block 
    split.h = *payload - (requested_size + 16); // remaining free space 
    split.prev = mystruct.prev;
    split.next = mystruct.next;
    *(curr_header *)split_address = split; 

    // Sets the size of the allocated block 
    *used = 16 + requested_size;
    *payload = requested_size;

    // Modifies the next and previous struct to update the double-linked free list pointers
    if (mystruct.next != NULL) {
        curr_header nextStruct = *(curr_header *)mystruct.next;
        nextStruct.prev = (size_t *)split_address;
        *(curr_header *)mystruct.next = nextStruct;
    }
    if (mystruct.prev != NULL) {
        curr_header prevStruct = *(curr_header *)mystruct.prev;
        prevStruct.next = (size_t *)split_address;
        *(curr_header *)mystruct.prev = prevStruct;                
    } else if (mystruct.prev  == NULL) { 
        // If this was the first free block, update the free list header 
        freeEnd = split_address;
    }
}

// This function removes a free block from the list without splitting, called when you can't efficiently split the block
void cantSplit(size_t *used, size_t *currentFree, size_t payload){ 
    curr_header mystruct = *(curr_header *)currentFree;
    *used = 16 + payload; 

    // Remove this block from the doubly-linked list 
    if (mystruct.prev != NULL) {
        curr_header prevStruct = *(curr_header *)mystruct.prev;
        prevStruct.next = mystruct.next;
        *(curr_header *)mystruct.prev = prevStruct;
    } else { 
        // This was the first free block 
        freeEnd = mystruct.next; // updates the free list head 
    } 

    if (mystruct.next != NULL) {
        curr_header nextStruct = *(curr_header *)mystruct.next;
        nextStruct.prev = mystruct.prev;
        *(curr_header *)mystruct.next = nextStruct;
    } else if (mystruct.next == NULL && mystruct.prev != NULL) { 
        // This was the last block, update the free list head to previous 
        freeEnd = mystruct.prev;
    } else if (mystruct.next == NULL && mystruct.prev == NULL) { 
        // This was the only free block and the free list is now empty 
        freeEnd = NULL;
    }
} 

// This function initializes the heap allocator 
bool myinit(void *heap_start, size_t heap_size) {
    // Sets up the initial state with one large free block covering the entire heap 
    if (heap_start == NULL) {
        return false;
    }
    
    // Initializes the global state variables 
    heapStart = heap_start;
    heapSize = heap_size;
    freeSpace = heapSize; 
    freeEnd = heapStart;
    sizeUsed = 0; 

    // Creates the initial free block header covering the entire heap 
    curr_header mystruct;
    mystruct.h = heap_size - 16; // available payload minus the header 
    mystruct.prev = NULL; // first block (has no previous)
    mystruct.next = NULL; // only block (no next)
    *(curr_header *)heapStart = mystruct; 

    return true; // returns true if the initialization is successfull and false otherwise 
}

// This function allocates a suitable block of memory from the heap 
void *mymalloc(size_t requested_size) {
    if (requested_size == 0) {
        return NULL; // returns NULL each time the allocation fails 
    }
    
    // Rounds up to maintain an 8-byte alignment 
    requested_size = roundup(requested_size); 

    // Checks if the request is valid and the requested size fits into the remaining heap space 
    if (requested_size > MAX_REQUEST_SIZE || (requested_size + sizeUsed) > heapSize) {
        return NULL;
    }

    size_t payload;
    size_t state;
    size_t space = freeSpace;
    size_t *currentFree = (size_t *)freeEnd; 

    // Traverses the free list to find a suitable block 
    while (space > 0) {
        if (currentFree == NULL) {
            break;
        } 

        curr_header mystruct = *(curr_header *)currentFree;
        size_t h = mystruct.h;
        payload = h;
        state = h & 1; // extracts the status bit 

        if (state == 0) { // the block is free 
            // Check if the block is large enough for the requested size
            if (requested_size <= MAX_REQUEST_SIZE && requested_size <= payload) {
                size_t used = 16 + payload;
                
                //Split the block is there is enough free space left over 
                if ((payload - requested_size) > 24) {
                    splitFunc(currentFree, &used, &payload, requested_size);
                } else { 
                    // Use the entire block without splitting 
                    cantSplit(&used, currentFree, payload);
                }
                
                // Updates global counter variables 
                sizeUsed += used;
                freeSpace -= used; 

                // Marks the block as allocated by setting the status bit 
                if ((payload & 1) == 0) {
                    mystruct.h = payload ^ 1; // sets the LSB to 1 
                } else {
                    printf("invalid payload");
                }
                *(curr_header *)currentFree = mystruct; 

                // Returns a pointer to the payload 
                unsigned char *payload_address = (unsigned char *)currentFree + 16;
                void *ptr = payload_address; 

                return ptr;
            }
        } else { 
            // Found allocated block in free list during malloc 
            printf("invalid address inside free list at malloc");
        } 

        space -= (16 + payload);
        currentFree = mystruct.next;
    }

    return NULL; // no suitable block found 
}

// This function frees a previously allocated block of memory and coalesces with right neighbour 
void myfree(void *ptr) { 
    if (ptr != NULL) { 
        //Gets the header of the block being freed 
        unsigned char *header = (unsigned char *)ptr - 16;
        curr_header mystruct = *(curr_header *)header;
        size_t payload = mystruct.h ^ 1; // clears allocated bit to get actual payload size 

        // Updates the global counters 
        size_t newPayload = 0;
        sizeUsed -= (payload + 16);
        freeSpace += (payload + 16);

        // Checks if the right neighbour can be coalesced 
        unsigned char *nextAddress = (unsigned char *)ptr + payload; 

        // Prevents reading beyond the bounds of the heap 
        if (nextAddress < (unsigned char *)heapStart + heapSize) {
            curr_header next_header = *(curr_header *)nextAddress;
            size_t next_payload = next_header.h;

            // Coalesces with right neighbour if its free 
            if ((next_payload & 1) == 0) { // right neighbour is free 
                newPayload = payload + (16 + next_payload);
                mystruct.h = newPayload; // combined payload size 
                mystruct.prev = next_header.prev;
                mystruct.next = next_header.next;
                *(curr_header *)header = mystruct;

                // Updates the free list pointers around the coalesced block 
                if (next_header.prev != NULL) {
                    curr_header prevStruct = *(curr_header *)next_header.prev;
                    prevStruct.next = (size_t *)header;
                    *(curr_header *)next_header.prev = prevStruct;
                }
            
                if (next_header.next != NULL) {
                    curr_header nextStruct = *(curr_header *)next_header.next;
                    nextStruct.prev = (size_t *)header;
                    *(curr_header *)next_header.next = nextStruct; 
                }

                // Changes freeList to reflect that mystruct is the first item in the list if necessary 
                if (next_header.prev == NULL) {
                    freeEnd = header;
                }
                return;
            }
        } else { // No coalescing possible 
            mystruct.h = payload; // Mark as free 
            mystruct.prev = NULL;
            mystruct.next = (size_t *)freeEnd; 

            if (freeEnd != NULL) {
            curr_header freeList = *(curr_header *)freeEnd;
            freeList.prev = (size_t *)header;
            *(curr_header *)freeEnd = freeList;
            } 

            *(curr_header *)header = mystruct;
            freeEnd = header; // this block becomes new freeList head
        }
    }
}

// This function reallocates a memory block to a new size 
void *myrealloc(void *old_ptr, size_t new_size) { 
    /* - Attempts in-place reallocation 
       - If it's not possible, falls back to the simple approach by:  
            - Finding new block (malloc)
            - Copying the data 
            - Freeing the old block  
    */ 

    // Handles the edge cases 
    if (new_size == 0 && old_ptr != NULL) {
         myfree(old_ptr);
         return old_ptr;
    }
    
    size_t requested_size = roundup(new_size);
    if (requested_size > MAX_REQUEST_SIZE || (requested_size + sizeUsed) > heapSize) {
        return NULL;
    }
    
    if (old_ptr == NULL) {
        return mymalloc(new_size);
    }
    
    // Try in-place realloc if the current payload is large enough
    unsigned char *old_pointer = (unsigned char *)old_ptr - 16;
    curr_header old_header = *(curr_header *)old_pointer;
    size_t old_payload = old_header.h ^ 1; // Get the actual payload size 
    
    if (requested_size <= old_payload) {
        return old_ptr; // Current block is sufficient 
    }
    
    // Allocates new payload because in-place realloc not possible 
    size_t payload;
    size_t space = freeSpace;
    size_t *currentFree = (size_t *)freeEnd;
   
    // Traverses heap to find a suitable block (malloc)
    while (space > 0) {
        if (currentFree == NULL) {
            break;
        }

        curr_header mystruct = *(curr_header *)currentFree;
        size_t h = mystruct.h;
        payload = h;
        size_t state = h & 1;

        if (state == 0) { 
            // Checks if the requested size is smaller than the payload
            if (requested_size <= MAX_REQUEST_SIZE && requested_size <= payload) {
                size_t used = 16 + payload;

                // Check if the payload can be split or whether to use entire block 
                if ((payload - requested_size) > 24) {
                    splitFunc(currentFree, &used, &payload, requested_size);
                } else {
                    cantSplit(&used, currentFree, payload);
                }
                
                // Updates the counters and marks block as allocated 
                sizeUsed += used;
                freeSpace -= used;
                if ((payload & 1) == 0) {
                    mystruct.h = payload ^ 1; // Sets allocated bit 
                } else {
                    printf("invalid payload");
                }
                *(curr_header *)currentFree = mystruct; 

                // Copies old data to new location and frees the old block 
                unsigned char *payload_address = (unsigned char *)currentFree + 16;
                void *ptr = payload_address;
                memmove(ptr, old_ptr, old_payload); // uses memmove for safe copying 
                myfree(old_ptr); 

                return ptr;
            }
        } else { 
            // Allocated block in free list 
            printf("invalid address inside free list at realloc");
        } 

        space -= (16 + payload);
        currentFree = mystruct.next;
    }

    return NULL; // no suitable block found 

}

// Validates the heap's consistency by checking the internal data structures 
bool validate_heap() {
    // Sanity check 
    if (sizeUsed > heapSize) {
        return false;
    } 

    // Traverse entire heap and verify blocks are accounted for  
    curr_header mystruct;
    size_t payload;
    size_t used = 0;
    size_t state;
    size_t frees = 0; 

    // Used to check size used and size free
    for (size_t i = 0; i < heapSize; i += 16) {
        unsigned char *nextIndex = (unsigned char *)heapStart + i;
        mystruct = *(curr_header *)nextIndex;
        state = mystruct.h & 1;
        payload = mystruct.h; 

        if (state == 1) { // allocated block 
            payload = mystruct.h ^ 1; // get actual payload size 
            used += (16 + payload);
        } else if (state == 0) { // free block 
            frees += (16 + payload);
        } 

        i += payload; // move to the next block 
    }
    
    // Used to calculate size free and check whether the freeList is accurate
    size_t *current = (size_t *)freeEnd;
    size_t freed = 0;
    size_t freePayload; 

    while (current != NULL) {
        curr_header freeStructs = *(curr_header *)current;
        freePayload = freeStructs.h; 

        // Free block shouldn't have allocated bit set 
        if ((freePayload & 1) == 1) {
            breakpoint();
            return false;
        } 

        freed += (freePayload + 16);
        current = freeStructs.next;
    } 

    // Verify consistency of accounting 
    if ((frees + used) != heapSize) {
        breakpoint();
        return false;
    } 

    if ((freeSpace + sizeUsed) != heapSize) {
        breakpoint();
        return false;
    } 

    return true;
}

// This file dumps the contents of the heap by printing out the diagnostic info of current heap 
void dump_heap() {
    printf("Heap starts at address %p and ends at %p. %lu bytes currently used.\n", heapStart, (char *)heapStart + heapSize, sizeUsed);
    
    size_t index = 0; 
    while (index < heapSize) {
        char *curr = (char *)heapStart + index;
        size_t *cur = (size_t *)curr;
        printf("The payload is: %zu", *cur);
        index = (*cur + 16);
    }
}