#define TEST_TIMEOUT 15
#include "__grading_helpers.h"
#include "debug.h"

/*
 * Check LIFO discipline on free list
 */
Test(sf_memsuite_grading, malloc_free_lifo, .timeout=TEST_TIMEOUT)
{
    size_t sz = 200;
    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 8), 1, 208);
    void * u = sf_malloc(sz);
    _assert_nonnull_payload_pointer(u);
    _assert_block_info((sf_block *)((char *)u - 8), 1, 208);
    void * y = sf_malloc(sz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 8), 1, 208);
    void * v = sf_malloc(sz);
    _assert_nonnull_payload_pointer(v);
    _assert_block_info((sf_block *)((char *)v - 8), 1, 208);
    void * z = sf_malloc(sz);
    _assert_nonnull_payload_pointer(z);
    _assert_block_info((sf_block *)((char *)z - 8), 1, 208);
    void * w = sf_malloc(sz);
    _assert_nonnull_payload_pointer(w);
    _assert_block_info((sf_block *)((char *)w - 8), 1, 208);

    sf_free(x);
    sf_free(y);
    sf_free(z);

    void * z1 = sf_malloc(sz);
    _assert_nonnull_payload_pointer(z1);
    _assert_block_info((sf_block *)((char *)z1 - 8), 1, 208);
    void * y1 = sf_malloc(sz);
    _assert_nonnull_payload_pointer(y1);
    _assert_block_info((sf_block *)((char *)y1 - 8), 1, 208);
    void * x1 = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x1);
    _assert_block_info((sf_block *)((char *)x1 - 8), 1, 208);

    cr_assert(x == x1 && y == y1 && z == z1,
      "malloc/free does not follow LIFO discipline");

    _assert_free_block_count(2808, 1);
    _assert_quick_list_block_count(0, 0);

    _assert_errno_eq(0);
}

/*
 * Realloc tests.
 */
Test(sf_memsuite_grading, realloc_larger, .timeout=TEST_TIMEOUT)
{
    size_t sz = 200;
    size_t nsz = 1024;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 8), 1, 208);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 8), 1, 1032);

    _assert_free_block_count(208, 1);
    _assert_quick_list_block_count(0, 0);
    _assert_free_block_count(2816, 1);

    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_smaller, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    size_t nsz = 200;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 8), 1, 1032);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 8), 1, 208);

    cr_assert_eq(x, y, "realloc to smaller size did not return same payload pointer");

    _assert_free_block_count(3848, 1);
    _assert_quick_list_block_count(0, 0);
    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_same, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    size_t nsz = 1024;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 8), 1, 1032);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 8), 1, 1032);

    cr_assert_eq(x, y, "realloc to same size did not return same payload pointer");
    _assert_free_block_count(3024, 1);

    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_splinter, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    size_t nsz = 1020;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 8), 1, 1032);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 8), 1, 1032);

    cr_assert_eq(x, y, "realloc to smaller size did not return same payload pointer");

    _assert_free_block_count(3024, 1);
    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_size_0, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 8), 1, 1032);

    void * y = sf_malloc(sz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 8), 1, 1032);

    void * z = sf_realloc(x, 0);
    _assert_null_payload_pointer(z);
    _assert_block_info((sf_block *)((char *)x - 8), 0, 1032);

    // after realloc x to (2) z, x is now a free block
    size_t exp_free_sz_x2z = 1032;
    _assert_free_block_count(exp_free_sz_x2z, 1);

    _assert_free_block_count(1992, 1);

    _assert_errno_eq(0);
}

/*
 * Illegal pointer tests.
 */
Test(sf_memsuite_grading, free_null, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    (void) sf_malloc(sz);
    sf_free(NULL);
    cr_assert_fail("SIGABRT should have been received");
}

//This test tests: Freeing a memory that was free-ed already
Test(sf_memsuite_grading, free_unallocated, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    void *x = sf_malloc(sz);
    sf_free(x);
    sf_free(x);
    cr_assert_fail("SIGABRT should have been received");
}

Test(sf_memsuite_grading, free_block_too_small, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    void * x = sf_malloc(sz);
    *((char *)x - 8) = 0x0UL;
    sf_free(x);
    cr_assert_fail("SIGABRT should have been received");
}


// random block assigments. Tried to give equal opportunity for each possible order to appear.
// But if the heap gets populated too quickly, try to make some space by realloc(half) existing
// allocated blocks.
Test(sf_memsuite_grading, stress_test, .timeout = TEST_TIMEOUT)
{
    errno = 0;

    int order_range = 13;
    int nullcount = 0;

    void * tracked[100];

    for (int i = 0; i < 100; i++)
    {
        int order = (rand() % order_range);
        size_t extra = (rand() % (1 << order));
        size_t req_sz = (1 << order) + extra;

        tracked[i] = sf_malloc(req_sz);
        // if there is no free to malloc
        if (tracked[i] == NULL)
        {
            order--;
            while (order >= 0)
            {
                req_sz = (1 << order) + (extra % (1 << order));
                tracked[i] = sf_malloc(req_sz);
                if (tracked[i] != NULL)
                {
                    break;
                }
                else
                {
                    order--;
                }
            }
        }

        // tracked[i] can still be NULL
        if (tracked[i] == NULL)
        {
            nullcount++;
            // It seems like there is not enough space in the heap.
            // Try to halve the size of each existing allocated block in the heap,
            // so that next mallocs possibly get free blocks.
            for (int j = 0; j < i; j++)
            {
                if (tracked[j] == NULL)
                {
                    continue;
                }
                sf_block * bp = (sf_block *)((char *)tracked[j] - 8);
                req_sz = (bp->header & ~0x7) >> 1;
                tracked[j] = sf_realloc(tracked[j], req_sz);
            }
        }
        errno = 0;
    }

    for (int i = 0; i < 100; i++)
    {
        if (tracked[i] != NULL)
        {
            sf_free(tracked[i]);
        }
    }

    _assert_heap_is_valid();

    // As allocations are random, there is a small probability that the entire heap
    // has not been used.  So only assert that there is one free block, not what size it is.
    //size_t exp_free_sz = MAX_SIZE - sizeof(_sf_prologue) - sizeof(_sf_epilogue);
    /*
     * We can't assert that there's only one big free block anymore because of
     * the quick lists. Blocks inside quick lists are still marked "allocated" so
     * they won't be coalesced with adjacent blocks.
     *
     * Let's comment this out. After all, _assert_heap_is_valid() will check uncoalesced
     * adjacent free blocks anyway.
     */
    // _assert_free_block_count(0, 1);
}
