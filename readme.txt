File: readme.txt
Author: DAVID WAMBUA KWOBA 
----------------------

implicit
--------
My implicit allocator uses three static variables to keep track of the information on the
heap. myinit checks whether the start address of the heap is valid before zeroing out the
other parameters in order to clear the heap. I use a pointer to store the information about
the header, including the size of the payload and whether the heap is free or allocated. For
mymalloc, I decided to loop through the entire heap and find the first block that fits the
specifications of the user. For myfree, I decided to just change the state of the ptr to show
that it is free given that the user could just overwrite the existing data and treat it as
garbage values. For my realloc, I also decided to use the same approach as mymalloc by looping
through the entire heap, as this involved minimal changes to the code. I also didn't think that
there would be a lot of benefits to the other implementations that I was considering, given
that I used the first fit method. I used memmove to transfer information from the old pointer
to the new pointer. My validate heap checked the state of the various variables and data in order
to ensure that the information was accurate. My dump heap implementation just printed out
the entire heap.

explicit
--------
For my explicit allocator, I used 5 static variables to store the information I needed for both
my validate_heap and myinit. I used freespace and sizeUsed to keep track of the various
allocations and deallocations. I used a static variable to always store the first thing in the
freeList, and to also ensure that I could update the freeList easily during allocations and
deallocation. I used structs to store the information about the payload, state, and the next
free items on the list as this approach seemed easier than calculating using pointers. I decided
to use a loop that looped until there was no item in the freeList by looping through the available
free space although I had previously tried an implementation that looped until the next item in
the freeList was a null_pointer. I then made sure to update the pointers and the freeList using
the static variables. Initially, my minimum heap size (the header and the payload) was 16, but
through my knowledge of assembly, I realized that data in certain heaps overwrote the data in the
subsequent heaps by pushing the values to the next heap if the number was a certain number of
bytes. As a result, I updated the value to a minimum of 24. I used print statements to know if
the code was hitting certain lines as part of my debugging strategy. For myfree, I implemented
coalesing with the next neighbor on the right, although the rest of the implementation remained
unchanged. I also had different implementations for when there was no coalesing required. My
reallocing function was just the same as my malloc, with a few key differences. I implemented
the option of reallocing in place for certain cases. I also moved the data to the new pointer and
freedthe old pointer to update the state of the heap. My validate heap kept track of the used
and freed up space to ensure that the information was accurate and track when bugs arose during
implementation. The dump_heap was just the same as the previous one.

