#define TEST_TIMEOUT 15
#include "__grading_helpers.h"
#include <assert.h>
#include "sfmm.h"

/*
 * Do one malloc and check that the prologue and epilogue are correctly initialized
 */
Test(sf_memsuite_grading, initialization, .timeout = TEST_TIMEOUT)
{
	size_t sz = 1;
	void *p  = sf_malloc(sz);
	cr_assert(p != NULL, "The pointer should NOT be null after a malloc");
	_assert_heap_is_valid();
}

/*
 * Single malloc tests, up to the size that forces a non-minimum block size.
 */
Test(sf_memsuite_grading, single_malloc_1, .timeout = TEST_TIMEOUT)
{

	size_t sz = 1;
	void *x = sf_malloc(sz);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 32);
	_assert_heap_is_valid();
	_assert_free_block_count(4024, 1);
	_assert_quick_list_block_count(0, 0);

	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, single_malloc_16, .timeout = TEST_TIMEOUT)
{
	size_t sz = 16;
	void *x = sf_malloc(sz);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 32);
	_assert_heap_is_valid();
	_assert_free_block_count(4024, 1);
	_assert_quick_list_block_count(0, 0);

	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, single_malloc_32, .timeout = TEST_TIMEOUT)
{
	size_t sz = 32;
	void *x = sf_malloc(sz);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 40);
	_assert_heap_is_valid();
	_assert_free_block_count(4016, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_errno_eq(0);
}


/*
 * Single malloc test, of a size exactly equal to what is left after initialization.
 * Requesting the exact remaining size (leaving space for the header)
 */
Test(sf_memsuite_grading, single_malloc_exactly_one_page_needed, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(4048);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 4056);
	_assert_heap_is_valid();
	_assert_free_block_count(0, 0);
	_assert_quick_list_block_count(0, 0);
	_assert_errno_eq(0);
}

/*
 * Single malloc test, of a size just larger than what is left after initialization.
 */
Test(sf_memsuite_grading, single_malloc_more_than_one_page_needed, .timeout = TEST_TIMEOUT)
{
	size_t sz = 4056;
	void *x = sf_malloc(sz);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 4064);
	_assert_heap_is_valid();	
	_assert_free_block_count(4088, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_errno_eq(0);
}

/*
 * Single malloc test, of multiple pages.
 */
Test(sf_memsuite_grading, single_malloc_three_pages_needed, .timeout = TEST_TIMEOUT)
{
	size_t sz = (size_t)((int)(PAGE_SZ * 3 / 1000) * 1000);
	void *x = sf_malloc(sz);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 12008);
	_assert_heap_is_valid();
	_assert_free_block_count(240, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_errno_eq(0);
}

/*
 * Single malloc test, unsatisfiable.
 * There should be one single large block.
 */
Test(sf_memsuite_grading, single_malloc_max, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(86016);
	_assert_null_payload_pointer(x);
	_assert_heap_is_valid();
	_assert_free_block_count(85976, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_errno_eq(ENOMEM);
}

/*
 * Malloc/free with/without coalescing.
 */
Test(sf_memsuite_grading, malloc_free_no_coalesce, .timeout = TEST_TIMEOUT)
{
        size_t sz1 = 200;
	size_t sz2 = 300;
	size_t sz3 = 400;

	void *x = sf_malloc(sz1);
	_assert_nonnull_payload_pointer(x);
	void *y = sf_malloc(sz2);
	_assert_nonnull_payload_pointer(y);
	void *z = sf_malloc(sz3);
	_assert_nonnull_payload_pointer(z);
	sf_free(y);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);
	_assert_block_info((sf_block *)((char *)y - 8), 0, 312);
	_assert_block_info((sf_block *)((char *)z - 8), 1, 408);
	_assert_heap_is_valid();

	_assert_free_block_count(312, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_free_block_count(3128, 1);
	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, malloc_free_coalesce_lower, .timeout = TEST_TIMEOUT)
{
	size_t sz1 = 200;
	size_t sz2 = 300;
	size_t sz3 = 400;
	size_t sz4 = 500;

	void *x = sf_malloc(sz1);
	sf_show_heap();
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);

	void *y = sf_malloc(sz2);
	sf_show_heap();
	_assert_nonnull_payload_pointer(y);
	_assert_block_info((sf_block *)((char *)y - 8), 1, 312);

	void *z = sf_malloc(sz3);
	sf_show_heap();
	_assert_nonnull_payload_pointer(z);
	_assert_block_info((sf_block *)((char *)z - 8), 1, 408);

	void *w = sf_malloc(sz4);
	sf_show_heap();
	_assert_nonnull_payload_pointer(w);
	_assert_block_info((sf_block *)((char *)w - 8), 1, 512);

	sf_free(y);
	sf_show_heap();
	sf_free(z);
	sf_show_heap();

	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);
	_assert_block_info((sf_block *)((char *)y - 8), 0, 720);
	_assert_block_info((sf_block *)((char *)w - 8), 1, 512);
	_assert_heap_is_valid();

	_assert_free_block_count(720, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_free_block_count(2616, 1);

	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, malloc_free_coalesce_upper, .timeout = TEST_TIMEOUT)
{
	size_t sz1 = 200;
	size_t sz2 = 300;
	size_t sz3 = 400;
	size_t sz4 = 500;

	void *x = sf_malloc(sz1);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);

	void *y = sf_malloc(sz2);
	_assert_nonnull_payload_pointer(y);
	_assert_block_info((sf_block *)((char *)y - 8), 1, 312);

	void *z = sf_malloc(sz3);
	_assert_nonnull_payload_pointer(z);
	_assert_block_info((sf_block *)((char *)z - 8), 1, 408);

	void *w = sf_malloc(sz4);
	_assert_nonnull_payload_pointer(w);
	_assert_block_info((sf_block *)((char *)w - 8), 1, 512);

	sf_free(z);
	sf_free(y);

	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);
	_assert_block_info((sf_block *)((char *)y - 8), 0, 720);
	_assert_block_info((sf_block *)((char *)w - 8), 1, 512);
	_assert_heap_is_valid();
	_assert_free_block_count(720, 1);
	_assert_quick_list_block_count(0, 0);

	_assert_free_block_count(2616, 1);

	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, malloc_free_coalesce_both, .timeout = TEST_TIMEOUT)
{
	size_t sz1 = 200;
	size_t sz2 = 300;
	size_t sz3 = 400;
	size_t sz4 = 500;

	void *x = sf_malloc(sz1);
	sf_show_heap();
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);
	

	void *y = sf_malloc(sz2);
	sf_show_heap();
	_assert_nonnull_payload_pointer(y);
	_assert_block_info((sf_block *)((char *)y - 8), 1, 312);

	void *z = sf_malloc(sz3);
	sf_show_heap();
	_assert_nonnull_payload_pointer(z);
	_assert_block_info((sf_block *)((char *)z - 8), 1, 408);

	void *w = sf_malloc(sz4);
	sf_show_heap();
	_assert_nonnull_payload_pointer(w);
	_assert_block_info((sf_block *)((char *)w - 8), 1, 512);

	sf_free(x);
	sf_show_heap();
	sf_free(z);
	sf_show_heap();
	sf_free(y);
	sf_show_heap();

	_assert_block_info((sf_block *)((char *)x - 8), 0, 928);
	_assert_heap_is_valid();
	_assert_free_block_count(928, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_free_block_count(2616, 1);

	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, malloc_free_first_block, .timeout = TEST_TIMEOUT)
{
	size_t sz1 = 200;
	size_t sz2 = 300;

	void *x = sf_malloc(sz1);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);

	void *y = sf_malloc(sz2);
	_assert_nonnull_payload_pointer(y);
	_assert_block_info((sf_block *)((char *)y - 8), 1, 312);

	sf_free(x);

	_assert_block_info((sf_block *)((char *)x - 8), 0, 208);
	_assert_block_info((sf_block *)((char *)y - 8), 1, 312);
	_assert_heap_is_valid();

	_assert_free_block_count(208, 1);
	_assert_quick_list_block_count(0, 0);
	_assert_free_block_count(3536, 1);

	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, malloc_free_last_block, .timeout = TEST_TIMEOUT)
{
	size_t sz1 = 200;
	size_t sz2 = 3840;
	void *x = sf_malloc(sz1);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);

	void *y = sf_malloc(sz2);
	_assert_nonnull_payload_pointer(y);
	_assert_block_info((sf_block *)((char *)y - 8), 1, 3848);

	sf_free(y);

	_assert_block_info((sf_block *)((char *)x - 8), 1, 208);
	_assert_block_info((sf_block *)((char *)y - 8), 0, 3848);
	_assert_free_block_count(3848, 1);
	_assert_quick_list_block_count(0, 0);

	_assert_heap_is_valid();

	_assert_errno_eq(0);
}

/*
 * Check that malloc leaves no splinter.
 */
Test(sf_memsuite_grading, malloc_with_splinter, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(4025);
	_assert_nonnull_payload_pointer(x);
	_assert_block_info((sf_block *)((char *)x - 8), 1, 4056);
	_assert_heap_is_valid();

	_assert_free_block_count(0, 0);
	_assert_quick_list_block_count(0, 0);

	_assert_errno_eq(0);
}

/*
 * Determine if the existing heap can satisfy an allocation request
 * of a specified size.  The heap blocks are examined directly;
 * freelists are ignored.
 */
static int can_satisfy_request(size_t size) {
    size_t asize = 112;
	if(asize < 32)
        asize = 32;
    sf_block *bp = ((sf_block *)(sf_mem_start() + 32));
    while(bp < (sf_block *)(sf_mem_end() - 8)) {
	if(!(bp->header & THIS_BLOCK_ALLOCATED) && asize <= (bp->header & ~0x7))
	    return 1;
	bp = (sf_block *)((char *)bp + (bp->header & ~0x7));
    }
    return 0;
}

/*
 *  Allocate small blocks until memory exhausted.
 */
Test(sf_memsuite_grading, malloc_to_exhaustion, .timeout = TEST_TIMEOUT)
{
    size_t size = 90;  // Arbitrarily chosen small size.
    size_t asize = 104;
    int limit = 10000;

    void *x;
    size_t bsize = 0;
    while(limit > 0 && (x = sf_malloc(size)) != NULL) {
	sf_block *bp = (sf_block *)((char *)x - 8);
	bsize = (bp->header & ~0x7);
	cr_assert(!bsize || bsize - asize < 32,
		  "Block has incorrect size (was: %lu, exp range: [%lu, %lu))",
		  bsize, asize, asize + 32);
	limit--;
    }
    cr_assert_null(x, "Allocation succeeded, but heap should be exhausted.");
    _assert_errno_eq(ENOMEM);
    cr_assert_null(sf_mem_grow(), "Allocation failed, but heap can still grow.");
    cr_assert(!can_satisfy_request(size),
	      "Allocation failed, but there is still a suitable free block.");
}

/*
 *  Test sf_memalign handling invalid arguments:
 *  If align is not a power of two or is less than the minimum block size,
 *  then NULL is returned and sf_errno is set to EINVAL.
 *  If size is 0, then NULL is returned without setting sf_errno.
 */
Test(sf_memsuite_grading, sf_memalign_test_1, .timeout = TEST_TIMEOUT)
{
    /* Test size is 0, then NULL is returned without setting sf_errno */
    int old_errno = sf_errno;  // Save the current errno just in case
    sf_errno = ENOTTY;  // Set errno to something it will never be set to as a test (in this case "not a typewriter")
    size_t arg_align = 32;
    size_t arg_size = 0;
    void *actual_ptr = sf_memalign(arg_size, arg_align);
    cr_assert_null(actual_ptr, "sf_memalign didn't return NULL when passed a size of 0");
    _assert_errno_eq(ENOTTY);  // Assert that the errno didn't change
    sf_errno = old_errno;  // Restore the old errno

    /* Test align less than the minimum block size */
    arg_align = 1U << 2;  // A power of 2 that is still less than MIN_BLOCK_SIZE
    arg_size = 25;  // Arbitrary
    actual_ptr = sf_memalign(arg_size, arg_align);
    cr_assert_null(actual_ptr, "sf_memalign didn't return NULL when passed align that was less than the minimum block size");
    _assert_errno_eq(EINVAL);

    /* Test align that isn't a power of 2 */
    arg_align = (32 << 1) - 1;  // Greater than 32, but not a power of 2
    arg_size = 65;  // Arbitrary
    actual_ptr = sf_memalign(arg_size, arg_align);
    cr_assert_null(actual_ptr, "sf_memalign didn't return NULL when passed align that wasn't a power of 2");
    _assert_errno_eq(EINVAL);
}

/*
Test that memalign returns an aligned address - using minimum block size for alignment
 */
Test(sf_memsuite_grading, sf_memalign_test_2, .timeout = TEST_TIMEOUT)
{
    size_t arg_align = 32;
    size_t arg_size = 25;  // Arbitrary
    void *x = sf_memalign(arg_size, arg_align);
    _assert_nonnull_payload_pointer(x);
    if (((unsigned long)x & (arg_align-1)) != 0) {
        cr_assert(1 == 0, "sf_memalign didn't return an aligned address!");
    }
}

/*
Test that memalign returns aligned, usable memory
 */
Test(sf_memsuite_grading, sf_memalign_test_3, .timeout = TEST_TIMEOUT)
{
    size_t arg_align = 32<<1; // Use larger than minimum block size for alignment
    size_t arg_size = 129;  // Arbitrary
    void *x = sf_memalign(arg_size, arg_align);
    _assert_nonnull_payload_pointer(x);
    if (((unsigned long)x & (arg_align-1)) != 0) {
        cr_assert(1 == 0, "sf_memalign didn't return an aligned address!");
    }
    _assert_heap_is_valid();
}

// Quick list tests
Test(sf_memsuite_grading, simple_quick_list, .timeout = TEST_TIMEOUT)
{

	size_t sz = 32;
	void *x = sf_malloc(sz);
	sf_free(x);  // Goes to quick list

	_assert_quick_list_block_count(0, 1);
	_assert_errno_eq(0);
}

Test(sf_memsuite_grading, multiple_quick_lists, .timeout = TEST_TIMEOUT)
{
	void *a = sf_malloc(32);
	void *b = sf_malloc(48);
	void *c = sf_malloc(96);
	void *d = sf_malloc(112);
	void *e = sf_malloc(128);

	sf_free(a);  // Goes to quick list
	sf_free(b);
	sf_free(c);
	sf_free(d);
	sf_free(e);

	_assert_quick_list_block_count(0, 5);
	_assert_errno_eq(0);
}
