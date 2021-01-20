/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
#include "config.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team-5",
    /* First member's full name */
    "Sujung Hwang",
    /* First member's email address */
    "panda1369@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};


/*Private global variables*/
//static char *mem_heap;          /*Points to first byte of heap*/
//static char *mem_brk;           /*Points to last byte of heap*/
//static char *mem_max_addr;      /*Max legal heap addr plus 1*/


/*Basic constants and macros*/
#define WSIZE 4             /*Word and header/footer size (bytes)*/
#define DSIZE 8             /*Double word size (bytes)*/
#define CHUNKSIZE (1<<12)   /*Extend heap by this amount (bytes)*/

#define MAX(x, y) ((x) > (y)? (x) : (y))          /*큰 값이 앞으로 와서 짝을 지어라?*/

/* Pack a size and allocated bit into a word */     /*뭔 지 모르겠음*/
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p*/
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p*/
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)     (GET(p) & 0x1)    

/* Given block ptr bp, compute address of its header and footer*/
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Giveb block ptr bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))
static char *heap_listp;
static char *next_fit;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

int mm_init(void)
{
 /* 
 *  mm_init - Create the initial empty heap
 *  최초 가용 블록으로 힙 생성하기
 */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);                                /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));       /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));       /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));           /* Epilogue header */
    heap_listp += (2*WSIZE);
    next_fit = heap_listp;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    /*
        새 가용 블록으로 힙 확장하기
    */

    char *bp;
    size_t size;

    /* Allocate an even numbers of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));          /*Free block header*/
    PUT(FTRP(bp), PACK(size, 0));           /*Free block footer*/
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /*New epilogue header*/

    /* Coalesce if the previous block was free*/                    /*Coalesce - 앞뒤봐서 남는공간 합치기*/
    return coalesce(bp);
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){                  /* Case 1 */
        return bp;
    }
    else if (prev_alloc && !next_alloc) {           /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    }
    else if (!prev_alloc && next_alloc) {           /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                          /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
        // next_fit
    if (next_fit >= (char *)bp && next_fit < NEXT_BLKP((char *)bp))
    {
        next_fit = bp;
    }
    return bp;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size*/
    size_t extendsize; /* Amount to extend heap if no fit */    /*공간이 모자라면 추가적으로 확장할 공간 크기*/
    char *bp;

    /* Ignore spurious requests : 안되는 요청 거절*/
    if (size == 0)
    {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    
    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);    /*size > DSIZE*/        

    /* Search the free list for a fit */
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memry and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize)
{   /* First-fit search */

    /*
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) 
        {
            return bp;
        }
    }
    return NULL;    //No fit
    */
   /* next-fit search */
char *bp = next_fit;
    // Search from next_fit to the end of the heap
    for (next_fit = bp; GET_SIZE(HDRP(next_fit)) > 0; next_fit = NEXT_BLKP(next_fit))
    {
        if (!GET_ALLOC(HDRP(next_fit)) && (asize <= GET_SIZE(HDRP(next_fit))))
        {
            // If a fit is found, return the address the of block pointer
            return next_fit;
        }
    }
    // If no fit is found by the end of the heap, start the search from the
    // beginning of the heap to the original next_fit location
    for (next_fit = heap_listp; next_fit < bp; next_fit = NEXT_BLKP(next_fit))
    {
        if (!GET_ALLOC(HDRP(next_fit)) && (asize <= GET_SIZE(HDRP(next_fit))))
        {
            return next_fit;
        }
    }
    return NULL; /* No fit */

}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){  
        /* 분할 후에 이 블록의 나머지가 최소 블록 크기(16 bytes)와 같거나 크다면 */
        /* asize 할당 후 남은 공간 분할 후 빈 공간으로 둠 */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        /*그냥 할당*/
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
#define SIZE_T_SIZE 8
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














