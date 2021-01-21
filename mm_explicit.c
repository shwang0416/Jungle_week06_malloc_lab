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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/**** 기본 상수 & 매크로 ****/
#define WSIZE 4             // Word Size
#define DSIZE 8             // Double-Word
#define CHUNKSIZE (1 << 12) // Extended heap 최대 확장 가능 크기

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // 헤더에 (size | alloc) : 비트 연산 하여 정보 저장

/* Read and Write a word at address p */
#define GET(p) (*(unsigned int *)(p))              // ptr에 저장된 워드 리턴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // ptr에 저장된 워드에 val 넣기

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // 사이즈 게산
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 정보

/* 블락 포인터 bp에 대해서, Header와 Footer의 주소를 던진다. */
#define HDRP(bp) ((char *)(bp)-WSIZE)                        // Header Pointer : payload ptr - 1 WORD
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // Footer Pointer : payload ptr + block SIZE - DSIZE(==2*WORD : 뒤로 2칸)

/* PRED_LOC & SUCC_LOC : PRED와 SUCC이 저장된 WORD */
#define PRED_LOC(bp) ((char *)HDRP(bp) + WSIZE)
#define SUCC_LOC(bp) ((char *)HDRP(bp) + DSIZE)

// 잘못됐다.
#define PUT_ADDRESS(p, adress) (*(char **)(p) = (adress))

/* PRED & SUCC : 해당 WORD에 저장된 값 불러오기 */

// PRED_LOC & SUCC_LOC 주소는 포인터를 저장하고 있는 곳의 주소이다.

// *(char *)PRED_LOC(bp)와 또 다르다.
// #define PRED(bp) (*(char **)PRED_LOC(bp))
// #define SUCC(bp) (*(char **)SUCC_LOC(bp))

//괄호 짝이 맞는지 잘 보자....
#define PRED(bp) (*(char **)(PRED_LOC(bp)))
#define SUCC(bp) (*(char **)(SUCC_LOC(bp)))
//#define PRED(bp) GET(PRED_LOC(bp))
//#define SUCC(bp) GET(SUCC_LOC(bp))

/* 블락 포인터 bp에 대해서, 다음 block 과 이전 block 얻어오기 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) // Next Block ptr : payload ptr + block Size (get from my Header)
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   // Prev Block ptr : payload ptr - prev block size (get from prev Footer)

static char *heap_listp;

// PUT_ADDRESS함수로 SUCC(bp)나 PRED(bp) 결과값 전달 불가능??
static void change(void *bp)
{
    // NULL 배열은 다음에만 올 수 있다.
    PUT_ADDRESS(SUCC_LOC(PRED(bp)), SUCC(bp));
    // 나의 전 녀석과, 나의 뒷 녀석을 건드린다.
    if (SUCC(bp) != NULL)
        PUT_ADDRESS(PRED_LOC(SUCC(bp)), PRED(bp));
}
/* 
    connect_root
    루트 => 나 => 루트's 이전 다음을 연결시키기
    (내가 둘 사이에 비집고 들어가는 것)
*/

static void connect_root(void *bp) {
// <1>  내 안에 양 root와 root의 이전다음놈 정보를 저장
    PUT(SUCC_LOC(bp), SUCC(heap_listp));
    PUT_ADDRESS(PRED_LOC(bp), heap_listp);
        
// <2> root와 root의 이전다음놈에게 내 정보를 줌
//  ※ 이때, root의 이전다음놈이 NULL이 아닌지 체크!
    if (SUCC(bp) != NULL) {
        PUT_ADDRESS(PRED_LOC(SUCC(bp)), bp);
    }
    PUT_ADDRESS(SUCC_LOC(PRED(bp)), bp);
}

/*
static void connect_root(void *bp)
{

    // 루트-> 나 -> 루트's 이전 다음 을 연결 시키기
    PUT_ADDRESS(PRED_LOC(bp), heap_listp);
    // printf("heap_listp : %p , PRED(bp) : %p\n", heap_listp, PRED(bp));

    PUT_ADDRESS(SUCC_LOC(bp), SUCC(heap_listp));
    // printf("SUCC(heap_listp) : %p , SUCC(bp) : %p\n", SUCC(heap_listp), SUCC(bp));

    if ((void *)SUCC(heap_listp) != NULL)
    {
        PUT_ADDRESS(PRED_LOC(SUCC(heap_listp)), bp);
    }
    PUT_ADDRESS(SUCC_LOC(heap_listp), bp);

}
*/

static void *coalesce(void *bp)
{   
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc)
    { /* Case 1 */
        connect_root(bp);   //<1>과 <2> 수행
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    { /* Case 2 */
        connect_root(bp);
        change(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        ////////////////////////////////////////////////////////////////////////////////////////////////
        /*
        // <1>
        // free한 놈의 succ을 root의 기존 succ으로 만든다.
        PUT(SUCC_LOC(bp), SUCC(heap_listp))
        // free한 놈의 pred를 root로 바꾼다.
        PUT_ADDRESS(PRED_LOC(bp), heap_listp)

        // <2>
        //(root의 기존 SUCC = )NEXT의 PRED를 free한 놈으로 바꾼다.
        PUT_ADDRESS(PRED_LOC(SUCC(heap_listp)), bp);
        // root의 SUCC을 free한 놈으로 바꾼다.
        PUT(SUCC_LOC(heap_listp), bp);
        */
       // <1> & <2>는 완전히 동일 => connect_root()

        /*
        // 기존에 free되어있던 놈(이제 합쳐질 놈)의 pred의 succ_loc에 합쳐질 놈의 succ을 준다.
        PUT(PRED_LOC(SUCC(NEXT_BLKP(bp))), PRED(NEXT_BLKP(bp)));
        // 기존에 free되어있던 놈(이제 합쳐질 놈)의 succ의 pred_loc에 합쳐질 놈의 pred를 준다.
        PUT(SUCC_LOC(PRED(NEXT_BLKP(bp))), SUCC(NEXT_BLKP(bp)));
        */
        // <3> : 합쳐진 놈의 SUCC과 PRED 간선 처리
        //      => change 함수에 bp가 아닌 합쳐진놈(NEXT_BLKP(bp) 전달)
    }
    else if (!prev_alloc && next_alloc)
    { 
        /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

        
        /*
        // ※ 간선 정보가 덮어씌워지면 안되므로 <3>을 먼저 진행함
        PUT(PRED_LOC(SUCC(bp)), PRED(bp));
        PUT(SUCC_LOC(SUCC(bp)), SUCC(bp));
        */
        change(bp);     // <3>

// <1> & <2>는 위와 동일함
        /*
        // <1>
        PUT(SUCC_LOC(bp), SUCC(heap_listp));
        PUT_ADDRESS(PRED_LOC(bp), heap_listp);

        // <2>
        PUT_ADDRESS(PRED_LOC(SUCC(bp)), bp);
        PUT_ADDRESS(SUCC_LOC(heap_listp), bp);
        */
       connect_root(bp);

    }
    else
    { 
        /* Case 4 */
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp))));

        // case 3처럼 bp를 앞으로 당겨주면 기존의 NEXT_BLKP로 접근할 방법이 사라지므로 old_next변수를 만들어 저장해둠
        char* old_next = NEXT_BLKP(bp);
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);             // bp 앞으로 당김

        //<3 - 1> (case 3의 <3>과 같음. 앞의 놈 간선처리)
        /*
        PUT(PRED_LOC(SUCC(bp)), PRED(bp));
        PUT(SUCC_LOC(SUCC(bp)), SUCC(bp));
        */
        change(bp);

        /*
        <old ptr>   : 이거 왜 필요??
            ➢ "root의 기존 SUCC"의 preD를 free 한놈으로 바꾼다.
            ➢ root의 SUCC를 "free한 놈"을 가리킨다.

        */
        //<3 - 2> (case 2의 <3>과 거의 같음. old_next를 이용해 뒤의 놈 간선처리)
        /*
        PUT(PRED_LOC(SUCC(old_next)), PRED(bp));
        PUT(SUCC_LOC(PRED(old_next)), SUCC(bp));
        */
       change(old_next);

// <1> & <2>는 위와 동일함
/*
        // <1>
        PUT(SUCC_LOC(bp), SUCC(heap_listp))
        PUT_ADDRESS(PRED_LOC(bp), heap_listp);

        // <2>
        PUT_ADDRESS(PRED_LOC(SUCC(bp)), bp);
        PUT_ADDRESS(SUCC_LOC(heap_listp), bp);
        */
       connect_root(bp);

    }
    
    return bp;
    //왜 필요??
    // 만약 할당이 꽉차게 됐다면 NULL을 반환하는데 
    // extend_heap에서 coalesce함수 자체를 리턴하기 때문
    // mm_malloc에서 extend_heap의 반환값을 보고 heap이 더이상 커질 수 있는지 판별한다.

    // => implicit에선 return bp를 지우면 문제가 생기는데 
    // explicit에서는 지워도 문제가 생기지 않았다. 
    // 이 로직에서 testcase가 extreme하지 않게 된건지
    // 아니면 꽉차게 되는 경우가 다른 부분에서 처리되고 있는건지는 모르겠음
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}
int mm_init(void)
{   

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);                                /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), NULL);
    PUT(heap_listp + (3 * WSIZE), NULL);
    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));         /* Epilogue header ** when find func, note endpoint */
    heap_listp += (2 * WSIZE);
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL){
        return -1;
    }
    PUT_ADDRESS(SUCC_LOC(heap_listp), NEXT_BLKP(heap_listp));
    /* root PRED */
    return 0;
}

static void *find_fit(size_t asize)
{
    char *bp;
    //아래 for 문에서 종료조건 : 
            // SUCC(bp)가 NULL인지 판단하고 NULL을 리턴해버리면
            // bp는 free공간이기 때문에 free인 공간인데도 더이상 남은공간이 없다고 return해버리는것!
            // 따라서 bp가 NULL인지 물어봐야 한다.
    for (bp = heap_listp; bp != NULL; bp = SUCC(bp))
    {   
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) 
        {
            return bp;
        }
    }
    return NULL;    //No fit

}
static size_t makeSize(size_t size)
{
    size_t asize; /* 적용될 size */

    /* Header & Footer + Align 조건 충족 */
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    }
        // 최소 4WORD는 필요
    else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    }
        // Header + Footer & 반올림

    return asize;
}
static void place(void *bp, size_t asize)
{
    size_t old_size = GET_SIZE(HDRP(bp));
    size_t diff = old_size - asize;
    // 분리해야 함

    if (diff >= (2 * DSIZE))                     // 분할하는 경우
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // bp => NEXT_BLKP(bp)로 PRED & SUCC 이식
        PUT_ADDRESS(SUCC_LOC(NEXT_BLKP(bp)), SUCC(bp));
        PUT_ADDRESS(PRED_LOC(NEXT_BLKP(bp)), PRED(bp));

        PUT_ADDRESS(SUCC_LOC((PRED(bp))), NEXT_BLKP(bp));
        if (SUCC(bp) != NULL) {
        //if (GET_SIZE(SUCC_LOC(bp)) != 0) { // bp의 SUCC이 Epilogue header가 아닐 때만
            PUT_ADDRESS(PRED_LOC(SUCC(bp)), NEXT_BLKP(bp));
        }

        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACK(diff, 0));
        PUT(FTRP(bp), PACK(diff, 0));
    }
    else                                                    //안하는 경우
    {
        change(bp);

        PUT(HDRP(bp), PACK(old_size, 1));
        PUT(FTRP(bp), PACK(old_size, 1));
    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }
    /* Adjust block size to include overhead and alignment reqs. */
    asize = makeSize(size);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    //printf("here free is\n");
    //printf("free : %p\n",bp);
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
    {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
/* 
 * mm_init - initialize the malloc package.
 */
