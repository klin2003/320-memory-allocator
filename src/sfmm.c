/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

int alignBy8(unsigned int allocSpace);
int findFreeIndex(unsigned int size);
sf_block *splitFreeBlock(sf_block *splitBlock, size_t bSize, int isFree);
void freeIntoFreeList(sf_block *blockPtr);
int coalesceBlocks();
void coalesceQuickBlocks(int qIndex);
void removeFreeBlock(sf_block *blckPtr, int fIndex);
void addFreeBlock(sf_block *blckPtr, int fIndex);
int invalidPtrCheck(sf_block *pp);
void initSentinel();

void *sf_malloc(size_t size) {
    // When implementing your sf_malloc function, first determine if the request size is 0.
    // If so, then return NULL without setting sf_errno
    if(size == 0)
        return NULL;


    /*
    If the request size is non-zero, then you should determine the size of the
    block to be allocated by adding the header size and the size of any necessary
    padding to reach a size that is a multiple of 8 to maintain proper alignment.
    */
    int blockSize = alignBy8(size); 

    /*
    After having determined the required block size, you should first check the
    quick lists to see if they contain a block of that size.
    */
    int qIndex = (blockSize - 32) / 8;
    if(qIndex < NUM_QUICK_LISTS && (sf_quick_lists[qIndex]).first != NULL){
        sf_block *qHead = sf_quick_lists[qIndex].first;
        
        // Clears the bit in "in qklst"
        qHead->header = (qHead->header & (~IN_QUICK_LIST)) | THIS_BLOCK_ALLOCATED;

        sf_quick_lists[qIndex].length--;
        if(sf_quick_lists[qIndex].length == 0)
            sf_quick_lists[qIndex].first = NULL;
        else
            sf_quick_lists[qIndex].first = qHead->body.links.next;

        // CHANGE PREV ALLOC HERE
        sf_header *nextHdr = (sf_header *)((char *)qHead + blockSize);
        *nextHdr = *nextHdr | PREV_BLOCK_ALLOCATED;

        return qHead->body.payload;
    }

    /*
    If they do not, you should determine the index of the first main free list
    that would be able to satisfy a request of that size.
    */
    else{
        int fIndex = findFreeIndex(blockSize);
        if(sf_free_list_heads[0].body.links.next == NULL || sf_free_list_heads[0].body.links.prev == NULL)
            initSentinel();
        
        // If there is no such block, continue with the next larger size class.
        while(fIndex < 10){
            sf_block *SENTINEL = &sf_free_list_heads[fIndex];

            sf_block *nextFree = SENTINEL->body.links.next;

            
            // Search that free list from the beginning until the first sufficiently large block is found.
            while(SENTINEL != nextFree){
                size_t freeSize = nextFree->header & (~0b111);
                if(freeSize >= blockSize){
                    /*
                    If a big enough block is found, then after splitting it (if it will not leave a splinter),
                    you should insert the remainder part back into the appropriate freelist.
                    */

                    if(freeSize - blockSize < 32)
                        blockSize = freeSize;

                    if(freeSize == blockSize){
                        // This indicates that no splitting is required
                        removeFreeBlock(nextFree, findFreeIndex(freeSize));
                        nextFree->header = (nextFree->header & (~IN_QUICK_LIST)) | THIS_BLOCK_ALLOCATED;
                    }
                    else{
                        /*
                        When splitting a block, the "lower part" (lower-valued addresses)
                        should be used to satisfy the allocation request and the "upper part"
                        (higher-valued addresses) should become the remainder.

                        Splitting is not considered a freeing operation, as such:

                        Do not insert this remainder portion into any quick list; it should be put
                        directly into the main free lists.
                        */
                        nextFree->header = (nextFree->header & (~IN_QUICK_LIST)) | THIS_BLOCK_ALLOCATED;
                        splitFreeBlock(nextFree, blockSize, 1);
                    }

                    // CHANGE PREV ALLOC HERE
                    sf_header *nextHdr = (sf_header *)((char *)nextFree + blockSize);
                    *nextHdr = *nextHdr | PREV_BLOCK_ALLOCATED;

                    return nextFree->body.payload;
                }
                nextFree = nextFree->body.links.next;
            }
            fIndex++;
        }
    }

    /*
    If a big enough block is not found in any of the freelists, then you must use sf_mem_grow to request more memory
    (for requests larger than a page, more than one such call might be required).
    */
    sf_block *epPtr = (sf_block *)((char *)sf_mem_end() - 8);
    void *endAddr = sf_mem_end();
    void *pagePtr = sf_mem_grow();
    char *memStart = (char *) sf_mem_start();

    /*
    When sf_mem_grow() returns NULL:

    If your allocator ultimately cannot satisfy the request, your sf_malloc function
    must set sf_errno to ENOMEM and return NULL.
    */
    if(pagePtr == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }

    // Padding if the heap is not constructed along our alignment boundaries
    if((size_t) memStart % 8 != 0)
        memStart += 8 - ((size_t) memStart % 8);
    if(sf_mem_start() == endAddr){
        // Add prologue if the first call to sf_mem_grow
        sf_block *heapHeader = (sf_block *) memStart;
        heapHeader->header = 32 | THIS_BLOCK_ALLOCATED;

        // Sets pointer to the next available space for a header
        sf_block *aftProlog = (sf_block *)((char *)heapHeader + 32);

        // Adjusts header to newly allocated memory, removing 32 and 8 for the prologue and epilogue
        aftProlog->header = ((char *)sf_mem_end() - (memStart + 32)) - 8;
        aftProlog->header = aftProlog->header | PREV_BLOCK_ALLOCATED;

        // Finds the footer for the block and copies the header
        sf_footer *initFooter = (sf_footer *)(((char *)aftProlog) + (aftProlog->header & (~0b111)) - 8);
        *initFooter = aftProlog->header;

        // Adds the newly created free block
        addFreeBlock(aftProlog, findFreeIndex(aftProlog->header & (~0b111)));
    }
    else{
        // Change the alloc bit in the old epilogue to 0 and change it to an appropriate header with the new 4096 bytes
        epPtr->header = epPtr->header & (~THIS_BLOCK_ALLOCATED);
        epPtr->header = PAGE_SZ | (epPtr->header & 0b111);

        // Set a corresponding footer to the header above
        sf_footer *tempEpFooter = (sf_footer *)((char *)epPtr + PAGE_SZ - 8);
        *tempEpFooter = epPtr->header;

        // Adds the newly created free block and coalesces the block with a possible prev free block
        addFreeBlock(epPtr, findFreeIndex(PAGE_SZ));
        coalesceBlocks(epPtr);
    }

    // Add new epilogue at the end of the new allocated memory
    sf_footer *heapFooter = (sf_footer *) ((char *)sf_mem_end() - 8);
    *heapFooter = 0b001;

    //sf_show_free_lists(); //TESTING
    return sf_malloc(size);
}

void sf_free(void *pp) {
    if(invalidPtrCheck(pp))
        abort();

    sf_block *blockPtr = (sf_block *) ((char *)pp - 8);
    sf_header blockSize = blockPtr->header & (~0b111);

    // If the block size matches the size of one of the quick lists
    int qIndex = (blockSize - 32) / 8;
    if(qIndex < NUM_QUICK_LISTS){
        if(sf_quick_lists[qIndex].length == QUICK_LIST_MAX){
            coalesceQuickBlocks(qIndex);
        }

        blockPtr->header = blockPtr->header | IN_QUICK_LIST | THIS_BLOCK_ALLOCATED;
        blockPtr->body.links.next = sf_quick_lists[qIndex].first;

        sf_quick_lists[qIndex].first = blockPtr;
        sf_quick_lists[qIndex].length++;

        //sf_show_quick_lists(); //TESTING
        
    }
    /*
    Otherwise, the block is inserted at the front of the appropriate
    main free list, after coalescing with any adjacent free block.
    Note that blocks in a main free list must not be marked as allocated,
    and they must have a valid footer with contents identical to the block header.
    */
    else{
        freeIntoFreeList(blockPtr);
    }

}

void *sf_realloc(void *pp, size_t rsize) {
    /*
    When implementing your sf_realloc function, you must first verify that the
    pointer passed to your function is valid. The criteria for pointer validity
    are the same as those described in the 'Notes on sf_free' section above.
    */

    sf_block* blockPtr = (sf_block *)((char *)pp - 8);

    if(invalidPtrCheck(pp)){
        sf_errno = EINVAL;
        return NULL;
    }

    // If the pointer is valid but the size parameter is 0, free the block and return NULL.
    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }
    
    size_t blockSize = blockPtr->header & (~0b111);
    size_t alignedSize = alignBy8(rsize);

    // Reallocing to LARGER size:

    if(blockSize < alignedSize){

        // 1. Call sf_malloc to obtain a larger block.
        sf_block *largeBlock = (sf_block *)((char *)sf_malloc(rsize) - 8);

        /*
        If sf_malloc returns NULL, sf_realloc must also return NULL. Note that
        you do not need to set sf_errno in sf_realloc because sf_malloc should
        take care of this.
        */
        if(largeBlock == NULL)
            return NULL;
        
    
        // 2. Call memcpy to copy the data in the block given by the client to the block
        // returned by sf_malloc.  Be sure to copy the entire payload area, but no more.
        memcpy(largeBlock->body.payload, blockPtr->body.payload, blockSize - 8);

        // 3. Call sf_free on the block given by the client (inserting into a quick list
        // or main freelist and coalescing if required).
        sf_free(pp);

     
        // 4. Return the block given to you by sf_malloc to the client.
        return largeBlock->body.payload;
    }

    // Reallocing to SMALLER size:

    else {
        /*
        Splitting the returned block results in a splinter. In this case, do not
        split the block. Leave the splinter in the block, update the header field
        if necessary, and return the same block back to the caller.

        Example:
                    b                                               b
        +----------------------+                       +------------------------+
        | allocated            |                       |   allocated.           |
        | Blocksize: 64 bytes  |   sf_realloc(b, 32)   |   Block size: 64 bytes |
        | payload: 48 bytes    |                       |   payload: 32 bytes    |
        |                      |                       |                        |
        |                      |                       |                        |
        +----------------------+                       +------------------------+

        In the example above, splitting the block would have caused a 24-byte splinter.
        Therefore, the block is not split.
        */
        if(blockSize - alignedSize < 32);

        /*
        The block can be split without creating a splinter. In this case, split the
        block and update the block size fields in both headers.  Free the remainder block
        by inserting it into the appropriate free list (after coalescing, if possible --
        do not insert the remainder block into a quick list).
        Return a pointer to the payload of the now-smaller block to the caller.

        Note that in both of these sub-cases, you return a pointer to the same block
        that was given to you.
        Example:
                    b                                              b
        +----------------------+                       +------------------------+
        | allocated            |                       | allocated |  free      |
        | Blocksize: 128 bytes |   sf_realloc(b, 50)   | 64 bytes  |  64 bytes. |
        | payload: 80 bytes    |                       | payload:  |            |
        |                      |                       | 50 bytes  | goes into  |
        |                      |                       |           | free list  |
        +----------------------+                       +------------------------+

        */
        else{
            splitFreeBlock(blockPtr, alignedSize, 0);
            sf_block *adjBlock = (sf_block *)((char *)blockPtr + blockSize);
            coalesceBlocks(adjBlock);
        }
        return blockPtr->body.payload;
    }
}

void *sf_memalign(size_t size, size_t align) {
    // By default, we return if the input size is 0
    if(size == 0)
        return NULL;

    /*
    When sf_memalign is called, it must check that the requested alignment is at
    least as large as the default alignment (8 bytes, for this assignment).
    It must also check that the requested alignment is a power of two. If either of
    these tests fail, sf_errno should be set to EINVAL and sf_memalign should return NULL.
    */
    if(align < 8 || (align & (align - 1)) != 0){
        sf_errno = EINVAL;
        return NULL;
    }

    // Should there be a case if align = 8

    /*
    In order to obtain memory with the requested alignment, sf_memalign allocates
    a larger block than requested.  Specifically, it attempts to allocate a block
    whose size is at least the requested size, plus the alignment size, plus the minimum
    block size, plus the size required for a block header and footer.
    */
    size_t largeSize = size; // Requested Size
    largeSize += align; // Align Size
    largeSize += 32; // Min Size
    // largeSize += 8; // Recall that our malloc will align the size to account for the header
    // largeSize += 8; // Recall that our implementation does not use footers for alloc blocks

    sf_block *largeBlock = (sf_block *)((char *)sf_malloc(largeSize) - 8);

    if(largeBlock == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }

    /*A block of this size will have the following property: either the normal payload address
    of the block will satisfy the requested alignment (in which case nothing further need be done),
    or else there will be a larger address within the block that satisfies the requested
    alignment, has sufficient space after it to hold the requested size payload,
    and in addition is sufficiently far from the beginning of the block that the initial
    portion of the block can itself be split off into a block of at least minimum size
    and freed:

            large block
    +-------------------------+                    +----+--------------------+
    |                         |                    |    |                    |
    |                         |    split 1         |    |                    |
    | hdr                 ftr |      ==>           |free|hdr payload     ftr |
    |                         |                    |  1 |    ^               |
    |                         |                    |    |    |               |
    +---------+---------------+                    +----+----+---------------+
            aligned                                          aligned

    */
   
    size_t origSize = largeBlock->header & (~0b111);
    size_t origAddr = (size_t)largeBlock + 8;
    size_t newAddr = origAddr;
    if(origAddr % align != 0){
        while(newAddr - origAddr < 32 || newAddr % align != 0){
            newAddr += align - (newAddr % align);
        }
        
        sf_block *newBlockAddr = (sf_block *)(newAddr - 8);
        newBlockAddr->header = (origSize - (newAddr - origAddr)) | 0b001;
        newBlockAddr->header = newBlockAddr->header | (largeBlock->header & PREV_BLOCK_ALLOCATED);

        // Change the header of the original largeBlock and free it
        largeBlock->header = (newAddr - origAddr) | (largeBlock->header & 0b111);
        freeIntoFreeList(largeBlock);
        
        largeBlock = newBlockAddr;
    }

    /*
    Once any initial portion of the block has been split off and freed, if the block
    is still too large for the requested size then it is subjected to splitting in
    the normal way (i.e. as in malloc) and the unused remainder is freed.
    +----+--------------------+                    +----+---------------+----+
    |    |                    |                    |    |               |    |
    |    |                    |                    |    |allocated block|    |
    |free|hdr payload     ftr |   split 2          |free|hdr payload ftr|free|
    |  1 |    ^               |     ==>            |  1 |    ^          |  2 |
    |    |    |               |                    |    |    |          |    |
    +----+----+---------------+                    +----+----+----------+----+
            aligned                                        aligned
    */
    
    // Call realloc on the given block to the designated size, this will decrease the size if possible
    
    void *newLargeBlock = sf_realloc(largeBlock->body.payload, size);
    largeBlock = (sf_block *)((char *)newLargeBlock - 8);

    if(largeBlock == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }

    return largeBlock->body.payload;
}

// NOTE: Align by will not account for splinters
int alignBy8(unsigned int allocSpace){
    int totalBytes = allocSpace;
    totalBytes += 8; // sf_header size should always be 8 bytes
    
    /*
    As has already been discussed, the above constraints lead to a minimum block size
    of 32 bytes, so you should not attempt to allocate any block smaller than this.
    */
    if(totalBytes < 32)
        return 32;

    if(totalBytes % 8 != 0)
        totalBytes = totalBytes + (8 - (totalBytes % 8));
    return totalBytes;
}

int findFreeIndex(unsigned int size){
    int fOutput = 0;
    int tempSize = size;
    while(tempSize >>= 1) fOutput++;
        
    unsigned int compExc = (1 << fOutput);
    if((size & ~compExc) > 0)
        fOutput++;
    
    fOutput -= 5;

    if(fOutput > 9)
        return 9;

    return fOutput;
}

// Function assumes that splitBlock is the header of the old block - isFree indicates if the old block was alloc or free
// Split block will allocate space from splitBlock and return the new address of the free block
sf_block *splitFreeBlock(sf_block *splitBlock, size_t bSize, int isFree){
    size_t freeSize = splitBlock->header & (~0b111);

    //fprintf(stderr, "\nBefore Split:\n"); //TESTING
    //sf_show_blocks(); //TESTING
    //sf_show_free_lists(); //TESTING
    if(isFree)
        removeFreeBlock(splitBlock, findFreeIndex(freeSize));

    // Updates the prev alloc bit of the next block to 0
    sf_block *adjBlock = ((sf_block *)(((char *) splitBlock) + freeSize));
    adjBlock->header = adjBlock->header & (~PREV_BLOCK_ALLOCATED);

    // Changes header for the new free block
    sf_block *newFreeBlock = ((sf_block *)(((char *) splitBlock) + bSize));
    newFreeBlock->header = ((freeSize - bSize) & (~0b111)) | PREV_BLOCK_ALLOCATED;

    // Copy header to footer
    sf_footer *newFooter = (sf_footer *)((char *)newFreeBlock + (freeSize - bSize) - 8);
    *newFooter = newFreeBlock->header;

    // Add new free block to the free list
    addFreeBlock(newFreeBlock, findFreeIndex(freeSize - bSize));

    // Changes header for the allocated block
    splitBlock->header = (bSize | (splitBlock->header & PREV_BLOCK_ALLOCATED)) | THIS_BLOCK_ALLOCATED;

    //fprintf(stderr, "\nAfter Split:\n"); //TESTING
    //sf_show_blocks(); //TESTING
    //sf_show_free_lists(); //TESTING
    return newFreeBlock;
}

void freeIntoFreeList(sf_block *blockPtr){
    size_t blockSize = blockPtr->header & (~0b111);

    // Create a new free block
    blockPtr->header = blockPtr->header & (~IN_QUICK_LIST) & (~THIS_BLOCK_ALLOCATED);
    sf_footer *nextFtr = (sf_footer *)((char *)blockPtr + blockSize - 8);
    *nextFtr = blockPtr->header;

    addFreeBlock(blockPtr, findFreeIndex(blockSize));

    sf_header *adjHeader = (sf_header *)((char *)blockPtr + blockSize);
    *adjHeader = *adjHeader & (~PREV_BLOCK_ALLOCATED);

    // If the next adjacent block is free
    if((*adjHeader & THIS_BLOCK_ALLOCATED) == 0)
        coalesceBlocks((sf_block *)adjHeader);

    coalesceBlocks(blockPtr);
}

// Coalesces a free block with the previous adjacent one
// DOES NOT coalesce the next adjacent one
int coalesceBlocks(sf_block *blckPtr){
    if((blckPtr->header & PREV_BLOCK_ALLOCATED) != 0)
        return 0;
    else{
        sf_footer *footPtr = (sf_footer *) (((char *)blckPtr) - 8);
        if((*footPtr & THIS_BLOCK_ALLOCATED) != 0)
            return -1;
        
        sf_footer blckSize = *footPtr & (~0b111);
        sf_footer currSize = blckPtr->header & (~0b111);
        sf_block *hdrAddr = (sf_block *) (((char *)blckPtr) - blckSize);
        int freeIndex;

        // Remove old blocks from the free list
        freeIndex = findFreeIndex(currSize);
        removeFreeBlock(blckPtr, freeIndex);
        freeIndex = findFreeIndex(blckSize);
        removeFreeBlock(hdrAddr, freeIndex);


        hdrAddr->header = ((hdrAddr->header & (~0b111)) + (blckPtr->header & (~0b111))) | (hdrAddr->header & 0b111);
        sf_footer *mergedFooter = (sf_footer *)((char *)hdrAddr + (hdrAddr->header & (~0b111)) - 8);
        *mergedFooter = hdrAddr->header;

        sf_header newBlckSize = hdrAddr->header & (~0b111);
        freeIndex = findFreeIndex(newBlckSize);
        addFreeBlock(hdrAddr, freeIndex);
    }
    return 1;
}

void coalesceQuickBlocks(int qIndex){
    sf_block *currQList = sf_quick_lists[qIndex].first;
    sf_block *nextQList;
    for(int i = 0; i < QUICK_LIST_MAX; i++){
        nextQList = currQList->body.links.next;
        currQList->header = currQList->header & (~0b101);
        
        sf_footer *newFooter = (sf_footer *)((char *)currQList + (currQList->header & (~0b111)) - 8);
        *newFooter = currQList->header;

        addFreeBlock(currQList, findFreeIndex((currQList->header & (~0b111))));

        sf_block *nextBlock = (sf_block *)((char *)currQList + (currQList->header & (~0b111)));
        nextBlock->header = nextBlock->header & (~PREV_BLOCK_ALLOCATED);
        
        if((nextBlock->header & THIS_BLOCK_ALLOCATED) == 0)
            coalesceBlocks(nextBlock);

        coalesceBlocks(currQList);

        currQList = nextQList;
    }
    
    sf_quick_lists[qIndex].first = NULL;
    sf_quick_lists[qIndex].length = 0;
}

void removeFreeBlock(sf_block *blckPtr, int fIndex){
    //fprintf(stderr, "\nBefore Remove:\n"); //TESTING
    //sf_show_free_lists(); //TESTING
    sf_block *nextBlock = sf_free_list_heads[fIndex].body.links.next;
    while(nextBlock != &sf_free_list_heads[fIndex]){
        if(nextBlock == blckPtr){
            nextBlock->body.links.prev->body.links.next = nextBlock->body.links.next;
            nextBlock->body.links.next->body.links.prev = nextBlock->body.links.prev;
            // nextBlock->body.links.next = NULL;
            // nextBlock->body.links.prev = NULL;
    //fprintf(stderr, "\nAfter Remove:\n"); //TESTING
    //sf_show_free_lists(); //TESTING
            return;
        }
        nextBlock = nextBlock->body.links.next;
    }
}

void addFreeBlock(sf_block *blckPtr, int fIndex){
    //fprintf(stderr, "\nBefore Add:\n"); //TESTING
    //sf_show_free_lists(); //TESTING
    sf_block *SENTINEL = &sf_free_list_heads[fIndex];
    blckPtr->body.links.next = SENTINEL->body.links.next;
    blckPtr->body.links.prev = SENTINEL;
    blckPtr->body.links.next->body.links.prev = blckPtr;
    SENTINEL->body.links.next = blckPtr;

    if(SENTINEL->body.links.prev == SENTINEL)
        SENTINEL->body.links.prev = blckPtr;
    //fprintf(stderr, "\nAfter Add:\n"); //TESTING
    //sf_show_free_lists(); //TESTING
}

int invalidPtrCheck(sf_block* pp){
    // - The pointer is NULL.
    if(pp == NULL)
        return 1;

    // - The pointer is not 8-byte aligned.
    if((size_t)pp % 8 != 0)
        return 1;
    
    sf_block *blockPtr = (sf_block *) ((char *)pp - 8);
    sf_header blockSize = blockPtr->header & (~0b111);

    // - The block size is less than the minimum block size of 32.
    // - The block size is not a multiple of 8
    if(blockSize < 32 || blockSize % 8 != 0)
        return 1;

    // - The header of the block is before the start of the first block of the heap,
    // or the footer of the block is after the end of the last block in the heap.
    if((char *)blockPtr < (char *)sf_mem_start() || ((char *)sf_mem_end() - ((char *)blockPtr + blockSize)) < 0){
        return 1;
    }

    // - The allocated bit in the header is 0.
    if((blockPtr->header & THIS_BLOCK_ALLOCATED) == 0)
        return 1;

    // - The in quick list bit in the header is 1.
    if((blockPtr->header & IN_QUICK_LIST) > 0)
        return 1;

    /*
    - The prev_alloc field in the header is 0, indicating that the previous
    block is free, but the alloc field of the previous block header is not 0.

    It is impossible to check the alloc field of the previous block header first.
    We must check the footer for the alloc bit instead to check for validity
    */
    if((blockPtr->header & PREV_BLOCK_ALLOCATED) == 0){
        sf_footer *ftrPtr = (sf_footer *)((char *)blockPtr - 8);
        if((*ftrPtr & THIS_BLOCK_ALLOCATED) > 0)
            return 1;
    }

    return 0;
}

void initSentinel(){
    for(int i = 0; i < NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}
