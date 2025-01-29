#include "__grading_helpers.h"
#include "debug.h"
#include "sfmm.h"

int free_list_idx(size_t size) {
    size_t s = 32;
    int i = 0;
    while(i < NUM_FREE_LISTS-1) {
        if(size <= s)
            return i;
        i++;
	s *= 2;
    }
    return NUM_FREE_LISTS-1;
}

static bool free_list_is_empty()
{
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	if(sf_free_list_heads[i].body.links.next != &sf_free_list_heads[i] ||
	   sf_free_list_heads[i].body.links.prev != &sf_free_list_heads[i])
	    return false;
   }
   return true;
}

static bool block_is_in_free_list(sf_block *abp)
{
    sf_block *bp = NULL;
    size_t size = abp->header & ~0x7;
    int i = free_list_idx(size);
    for(bp = sf_free_list_heads[i].body.links.next; bp != &sf_free_list_heads[i];
        bp = bp->body.links.next) {
            if (bp == abp)  return true;
    }
    return false;
}

void _assert_free_list_is_empty()
{
    cr_assert(free_list_is_empty(), "Free list is not empty");
}

/*
 * Function checks if all blocks are unallocated in free_list.
 * This function does not check if the block belongs in the specified free_list.
 */
void _assert_free_list_is_valid()
{
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_block *bp = sf_free_list_heads[i].body.links.next;
        int limit = 10000;
        while(bp != &sf_free_list_heads[i] && limit--) {
            cr_assert(!(bp->header & THIS_BLOCK_ALLOCATED),
		      "Block %p in free list is marked allocated", &(bp)->header);
            bp = bp->body.links.next;
        }
        cr_assert(limit != 0, "Bad free list");
    }
}

/**
 * Checks if a block follows documentation constraints
 *
 * @param bp pointer to the block to check
 */
void _assert_block_is_valid(sf_block *bp)
{
   // proper block alignment & size
   size_t unaligned_payload = ((size_t)(&bp->body.payload) & 0x7);
   cr_assert(!unaligned_payload,
      "Block %p is not properly aligned", &(bp)->header);
    size_t block_size = bp->header & ~0x7;

   cr_assert(block_size >= 32 && block_size <= 409600 && !(block_size & 0x7),
      "Block size is invalid for %p. Got: %lu", &(bp)->header, block_size);

   // prev alloc bit of next block == alloc bit of this block
   sf_block *nbp = (sf_block *)((char *)bp + block_size);

   cr_assert(nbp == ((sf_block *)(sf_mem_end() - 8)) ||
	     (((nbp->header & PREV_BLOCK_ALLOCATED) != 0) == ((bp->header & THIS_BLOCK_ALLOCATED) != 0)),
	     "Prev allocated bit is not correctly set for %p. Should be: %d",
	     &(nbp)->header, bp->header & THIS_BLOCK_ALLOCATED);

   // other issues to check
   sf_footer *footer = (sf_footer *)((char *)(nbp) - sizeof(sf_footer));
   cr_assert((bp->header & THIS_BLOCK_ALLOCATED) || (*footer == bp->header),
	     "block's footer does not match header for %p", &(bp)->header);
   if (bp->header & THIS_BLOCK_ALLOCATED) {
       cr_assert(!block_is_in_free_list(bp),
		 "Allocated block at %p is also in the free list", &(bp)->header);
   } else {
       cr_assert(block_is_in_free_list(bp),
		 "Free block at %p is not contained in the free list", &(bp)->header);
   }
}

void _assert_heap_is_valid(void)
{
    sf_block *bp;
    int limit = 10000;

    if(sf_mem_start() == sf_mem_end())
        cr_assert(free_list_is_empty(), "The heap is empty, but the free list is not");
    for(bp = ((sf_block *)(sf_mem_start() + 32)); limit-- && bp < ((sf_block *)(sf_mem_end() - 8));
	bp = (sf_block *)((char *)bp + (bp->header & ~0x7)))
	_assert_block_is_valid(bp);
    cr_assert(bp == ((sf_block *)(sf_mem_end() - 8)), "Could not traverse entire heap");
    _assert_free_list_is_valid();
}

/**
 * Asserts a block's info.
 *
 * @param bp pointer to the beginning of the block.
 * @param alloc The expected allocation bit for the block.
 * @param b_size The expected block size.
 */
void _assert_block_info(sf_block *bp, int alloc, size_t b_size)
{
    cr_assert((bp->header & THIS_BLOCK_ALLOCATED) == alloc,
	      "Block %p has wrong allocation status (got %d, expected %d)",
	      &(bp)->header, bp->header & THIS_BLOCK_ALLOCATED, alloc);

    cr_assert((bp->header & ~0x7) == b_size,
	      "Block %p has wrong block_size (got %lu, expected %lu)",
	      &(bp)->header, (bp->header & ~0x7), b_size);
}

/**
 * Asserts payload pointer is not null.
 *
 * @param pp payload pointer.
 */
void _assert_nonnull_payload_pointer(void *pp)
{
    cr_assert(pp != NULL, "Payload pointer should not be NULL");
}

/**
 * Asserts payload pointer is null.
 *
 * @param pp payload pointer.
 */
void _assert_null_payload_pointer(void * pp)
{
    cr_assert(pp == NULL, "Payload pointer should be NULL");
}

/**
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 *
 * @param size the size of free blocks to count.
 * @param count the expected number of free blocks to be counted.
 */
void _assert_free_block_count(size_t size, int count)
{
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_block *bp = sf_free_list_heads[i].body.links.next;
        while(bp != &sf_free_list_heads[i]) {
            if(size == 0 || size == (bp->header & ~0x7))
                cnt++;
            bp = bp->body.links.next;
        }
    }
    if(size)
        cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    else
        cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
}


/*
 * Assert the total number of quick list blocks of a specified size.
 * If size == 0, then assert the total number of all quick list blocks.
 */
void _assert_quick_list_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_QUICK_LISTS; i++) {
        sf_block *bp = sf_quick_lists[i].first;
        while(bp != NULL) {
            if(size == 0 || size == (bp->header & ~0x7))
                cnt++;
            bp = bp->body.links.next;
        }
    }
    if(size == 0) {
    cr_assert_eq(cnt, count, "Wrong number of quick list blocks (exp=%d, found=%d)",
             count, cnt);
    } else {
    cr_assert_eq(cnt, count, "Wrong number of quick list blocks of size %ld (exp=%d, found=%d)",
             size, count, cnt);
    }
}

/**
 * Assert the sf_errno.
 *
 * @param n the errno expected
 */
void _assert_errno_eq(int n)
{
    cr_assert_eq(sf_errno, n, "sf_errno has incorrect value (value=%d, exp=%d)", sf_errno, n);
}
