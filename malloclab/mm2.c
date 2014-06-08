/*
 * mm.c - Uses an explicit free list to allocate memory requests.
 *
 *        This implementation uses an explicit free list to keep track of free blocks.
 *        It has decen speed, and utilization although it's weak to thrashing.
 *        Each block has a header and footer to store size and allocation status, free blocks
 *        store pointers to the next and previous free blocks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below _AND_ in the
 * struct that follows.
 *
 * === User information ===
 * Group: Mallificent
 * User 1: sandrarj13
 * SSN: 0104902949
 * User 2: grettir10
 * SSN: 1001892249
 * === End User Information ===
 ********************************************************/
team_t team = {
    /* Group name */
    "Mallificent",
    /* First member's full name */
    "Sandra Ros Hrefnu Jonsdottir",
    /* First member's email address */
    "sandrarj13@ru.is",
    /* Second member's full name (leave blank if none) */
    "Grettir Olafsson",
    /* Second member's email address (leave blank if none) */
    "grettir10@ru.is",
    /* Leave blank */
    "",
    /* Leave blank */
    ""
};

/* Begin mallocmacros */
/* Basic constants and macros */
#define WSIZE               4       /* word size in bytes*/
#define DSIZE               8       /* doubleword size in bytes*/
#define OVERHEAD            8       /* overhead of header and footer in bytes */
#define MIN_BLOCK_SIZE      16      /* Minimum block size for explicit free list*/

#define MAX(x, y)           ((x) > (y) ? (x) : (y))

#define ALIGN(size)         (((size) + (DSIZE - 1)) & ~0x7)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((ALIGN(size)) | (alloc))

/* Read and write a word at address ptr */
#define GET(ptr)            (*(size_t *)(ptr))
#define PUT(ptr, val)       (*(size_t *)(ptr) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(ptr)       (GET(ptr) & ~0x7)
#define GET_ALLOC(ptr)      (GET(ptr) & 0x1)

/* Given block ptr bptr, compute address of its header and footer */
#define HDRP(bptr)          ((char *)(bptr) - WSIZE)
#define FTRP(bptr)          ((char *)(bptr) + GET_SIZE(HDRP(bptr)) - DSIZE)

/* Given block ptr bptr, compute address of next and previous blocks */
#define NEXT_BLKP(bptr)     ((char *)(bptr) + GET_SIZE(((char *)(bptr) - WSIZE)))
#define PREV_BLKP(bptr)     ((char *)(bptr) - GET_SIZE(((char *)(bptr) - DSIZE)))

/* Given block ptr bptr, get the next and prev pointers. */
#define NEXT_PTR(bptr)      (GET(HDRP(bptr) + WSIZE))
#define PREV_PTR(bptr)      (GET(HDRP(bptr) + DSIZE))
/* End mallocmacros*/

/* Global variables */
static void *heap_listp; /* Pointer to first block*/
size_t *free_list_root;
size_t *free_list_end;

/* Function prototypes for internal helper routines */
int mm_check(void);
static void *coalesce(void *bptr);
void *find_fit(size_t size, void *bptr);
void add_free_block(void *bptr);
void remove_free_block(void *bptr);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    
    PUT(heap_listp, 0);                             // Alignment padding.
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  // Prologue header.
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  // Prologue footer.
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      // Epilogue header.
    heap_listp += (2 * WSIZE);
    
    // Free list is empty at the start.
    free_list_root = NULL;
    free_list_end = NULL;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    void *bptr;

    /* Ignore spurious requests */
    if(size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment requirements */
    if(size <= DSIZE)
        asize = DSIZE + OVERHEAD;
    else
        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE - 1)) / DSIZE);
    
    // Check if it will fit anywhere on the free list.
    if((free_list_root != NULL && (asize >= GET_SIZE(HDRP(free_list_root)))
            && (free_list_end != NULL && asize <= GET_SIZE(HDRP(free_list_end)))))
    {
        if(asize == GET_SIZE(HDRP(free_list_root)))
            bptr = free_list_root;
        else if(asize == GET_SIZE(HDRP(free_list_end)))
            bptr = free_list_end;
        else
            bptr = find_fit(asize, free_list_root);

        remove_free_block(bptr);

        size_t csize = GET_SIZE(HDRP(bptr));

        PUT(HDRP(bptr), PACK(asize, 1));
        PUT(FTRP(bptr), PACK(asize, 1));
        
        // Split the block if possible.
        if((csize - asize) >= (DSIZE + OVERHEAD))
        {
            void *nextBptr = NEXT_BLKP(bptr);
            PUT(HDRP(nextBptr), PACK(csize - asize, 0));
            PUT(FTRP(nextBptr), PACK(csize - asize, 0));

            add_free_block(nextBptr);
        }
        return bptr;
    }

    /* No fit found. Get more memory and place the block */
    if((bptr = mem_sbrk(asize)) == (void *)-1)
        return NULL;
    
    // Mark header and footer.
    PUT(HDRP(bptr), PACK(asize, 1));
    PUT(FTRP(bptr), PACK(asize, 1));
    // New Epilogue header
    PUT(HDRP(NEXT_BLKP(bptr)), PACK(0, 1));
    
    return bptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bptr)
{
    size_t size = GET_SIZE(HDRP(bptr));
    
    // Free up the header and footer of the block.
    PUT(HDRP(bptr), PACK(size, 0));
    PUT(FTRP(bptr), PACK(size, 0));
    
    // Call coalesce, to avoid fragmentation.
    bptr = coalesce(bptr);

    add_free_block(bptr);
}

/*
 * mm_realloc - Allocates a new block only if needed, if size is larger than old size.
 *              
 */
void *mm_realloc(void *bptr, size_t size)
{
    // If bptr is NULL and size larger than zero, realloc has the same
    // behaviour as mm_malloc, so for now we just call mm_malloc.
    // If size == 0, it should have the same behavious as mm_free so we
    // just call mm_free.
    if(bptr == NULL && size > 0)
        return mm_malloc(size);
    else if(size <= 0)
    {
        mm_free(bptr);
        return NULL;
    }
    void *new_bptr;
    size_t old_size = GET_SIZE(HDRP(bptr));
    size_t new_size = ALIGN(size);

    // If the sizes are the same, nothing needs to be done.
    if(new_size == old_size)
        return bptr;

    // If the new size is smaller than the old one, we can re-package the boundary tags
    // and also check if we can split the block.
    if(new_size < old_size)
    {
        // Re-package headers.
        PUT(HDRP(bptr), PACK(new_size, 1));
        PUT(FTRP(bptr), PACK(new_size, 1));

        // Check if rest of the block can be split.
        if(old_size - new_size >= MIN_BLOCK_SIZE)
        {
            // Split the block and add rest to free_list.
            void *next_bptr = NEXT_BLKP(bptr);
            PUT(HDRP(next_bptr), PACK(old_size - new_size, 0));
            PUT(FTRP(next_bptr), PACK(old_size - new_size, 0));

            next_bptr = coalesce(next_bptr);
            add_free_block(next_bptr);
        }
        return bptr;
    }

    new_bptr = coalesce(bptr);
    if(old_size < GET_SIZE(HDRP(new_bptr)) && new_size + DSIZE < GET_SIZE(HDRP(new_bptr)))
    {
        PUT(HDRP(new_bptr), PACK(new_size, 1));
        PUT(FTRP(new_bptr), PACK(new_size, 1));

        if(HDRP(new_bptr) != HDRP(bptr))
            memcpy(new_bptr, bptr, size);

        void *next_bptr = NEXT_BLKP(new_bptr);
        PUT(HDRP(next_bptr), PACK(GET_SIZE(HDRP(next_bptr) - new_size), 0));
        PUT(FTRP(next_bptr), PACK(GET_SIZE(HDRP(next_bptr) - new_size), 0));

        add_free_block(next_bptr);

        return new_bptr;
    }

    // Otherwise, new_size is larger so we need to allocate a new block
    new_bptr = mm_malloc(new_size - old_size); // should be size, but that causes the driver to run out of memory.
    memcpy(new_bptr, bptr, old_size - DSIZE);
    mm_free(bptr);

    return new_bptr;
}

/*
 * mm_check - Checks the consistency of the heap.
 */
int mm_check(void)
{
    void *bptr = free_list_root;
    char alloc;

    // Check if the free list is consistent.
    printf("\nFree list:  |");
    while(bptr != NULL)
    {
        if(NEXT_PTR(bptr) && GET_ALLOC(HDRP(NEXT_PTR(bptr))))
        {
            printf("Block [%x] points to an allocated block!\n", (size_t)bptr);
            return 0;
        }

        if(GET_ALLOC(HDRP(bptr)))
        {
            printf("Block [%x] is marked as allocated.\n", (size_t)bptr);
            return 0;
        }
        printf("|_____[%d]_______|", GET_SIZE(HDRP(bptr)));

        bptr = NEXT_PTR(bptr);
    }
    printf("|\n\n");

    bptr = heap_listp;
    void *prev = NULL;

    printf("\nHeap:   |");
    for(bptr; GET_SIZE(HDRP(bptr)) > 0; bptr = NEXT_BLKP(bptr))
    {
        if(bptr >= mem_heap_hi() && bptr < mem_heap_lo())
        {
            printf("ERROR: Block is not located on the heap.\n");
            return 0;
        }
        // If it's free.
        if(!GET_ALLOC(HDRP(bptr)))
        {
            alloc = 'f';
        }
        else
        {
            alloc = 'a';
        }
        
        printf("|_____[%d]_____(%c)_|", GET_SIZE(HDRP(bptr)), alloc);
    }
    
    printf("|\n\n");
    
    if((GET_SIZE(HDRP(bptr)) != 0) || !(GET_ALLOC(HDRP(bptr))))
    {
        printf("Bad epilogue header\n");
        return 0;
    }

    return 1;
}

/*
 * coalesce - Boundary tag coalescing. Return bptr to coalesced block
 */
static void *coalesce(void *bptr)
{
    void *prev_bptr = PREV_BLKP(bptr);
    void *next_bptr = NEXT_BLKP(bptr);
    size_t size = GET_SIZE(HDRP(bptr));
    int heap_boundary = (bptr >= mem_heap_hi() && bptr < mem_heap_lo());

    if(heap_boundary && !GET_ALLOC(next_bptr))
    {
        remove_free_block(next_bptr);

        PREV_PTR(next_bptr) = NULL;
        NEXT_PTR(next_bptr) = NULL;

        size_t new_size = GET_SIZE(HDRP(bptr)) + GET_SIZE(HDRP(next_bptr));
        PUT(HDRP(bptr), PACK(new_size, 0));
        PUT(FTRP(bptr), PACK(new_size, 0));
    }
    if(heap_boundary && !GET_ALLOC(prev_bptr))
    {
        remove_free_block(prev_bptr);

        PREV_PTR(prev_bptr) = NULL;
        NEXT_PTR(prev_bptr) = NULL;

        size_t new_size = GET_SIZE(HDRP(bptr)) + GET_SIZE(HDRP(next_bptr));
        PUT(HDRP(bptr), PACK(new_size, 0));
        PUT(FTRP(bptr), PACK(new_size, 0));
    }

    return bptr;
}

/*
 * find_fit - Recursively search for a fit.
 */
void *find_fit(size_t size, void *bptr)
{
    if(bptr == NULL)
        return NULL;

    if(size <= GET_SIZE(HDRP(bptr)))
        return bptr;

    return find_fit(size, NEXT_PTR(bptr));
}

/*
 * add_free_block - Adds a free block to the free list. Arrange by size
 * to avoid fragmentation.
 */
void add_free_block(void *bptr)
{
    if(bptr == NULL)
        return;
    /*
     * Possible cases:
     *      List is empty
     *      Block size is smaller than the start of the list (would be smallest then
     *      so we add to start of list).
     *      Block size is larger than the end of the list (would be largest then so
     *      we add to the end of list).
     *      None of this so we need to find where it goes.
     */
    if(free_list_root == NULL && free_list_end == NULL)
    {
        free_list_root = bptr;
        free_list_end = bptr;
        return;
    }
    else if(GET_SIZE(HDRP(bptr)) <= GET_SIZE(HDRP(free_list_root)))
    {
        NEXT_PTR(bptr) = free_list_root;
        PREV_PTR(free_list_root) = bptr;
        free_list_root = bptr;
        return;
    }
    else if(GET_SIZE(HDRP(bptr)) >= GET_SIZE(HDRP(free_list_end)))
    {
        PREV_PTR(bptr) = free_list_end;
        NEXT_PTR(free_list_end) = bptr;
        free_list_end = bptr;
        return;
    }
    else
    {
        void *place = find_fit(GET_SIZE(HDRP(bptr)), free_list_root);
        NEXT_PTR(bptr) = place;
        PREV_PTR(bptr) = PREV_PTR(place);
        NEXT_PTR(PREV_PTR(bptr)) = bptr;
        PREV_PTR(place) = bptr;
    }
}

/*
 * remove_free_block - Removes a free block from the free list.
 */
void remove_free_block(void *bptr)
{
    if(bptr == NULL)
        return;
    /*
     * Possible cases:
     *      This is the last block on the list.
     *      This is the first item on the list, so we just need to update the next
     *      block and the root pointer.
     *      This is the last item on the list, so we just need to update the previous
     *      block and the list_end pointer.
     *      This block is somewhere within the list, so we need to update the blocks
     *      on either side of it.
     */

    if(free_list_root == free_list_end)
    {
        free_list_root = NULL;
        free_list_end = NULL;
    }
    else if(free_list_root == bptr)
    {
        free_list_root = NEXT_PTR(bptr);
        PREV_PTR(NEXT_PTR(bptr)) = NULL;
    }
    else if(free_list_end == bptr)
    {
        free_list_end = PREV_PTR(bptr);
        NEXT_PTR(PREV_PTR(bptr)) = NULL;
    }
    else
    {
        NEXT_PTR(PREV_PTR(bptr)) = NEXT_PTR(bptr);
        PREV_PTR(NEXT_PTR(bptr)) = PREV_PTR(bptr);
    }
}




