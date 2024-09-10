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
    "team 4",
    /* First member's full name */
    "Wangyu",
    /* First member's email address */
    "wangyu7958@gmail.com",
    /* Second member's full name (leave blank if none) */
    "EunSang HyeDa",
    /* Second member's email address (leave blank if none) */
    "EunHye@naver.com"
};

void *heap_listp = NULL;
void *linked_listp = NULL;      //가용블럭 연결리스트의 첫 가용블럭을 가리키는 포인터 변수

void *extend_heap(size_t words);
void *find_fit(size_t asize);
void place(void *bp, size_t asize);
void insert_linked(void *bp);
void delete_linked(void *bp);
void *coalesce(void *bp);

#define WSIZE 4
#define DSIZE 8

#define CHUNKSIZE       (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)          (*(unsigned int *)(p))

#define PUT(p, val)     (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)     (GET(p) & ~0x7)

#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)        ((char *)(bp) - WSIZE)

#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))

#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PREV_LINK(bp)   (*(void **)(bp))                    //가용블럭의 PREV에 저장된 주소를 반환

#define NEXT_LINK(bp)   (*(void **)(char *)(bp + WSIZE))    //가용블럭의 NEXT에 저장된 주소를 반환
         
int mm_init(void)
{ 
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1){
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + 1 * WSIZE, PACK(2 * DSIZE, 1));    //8바이트가 아니라 16바이트로 설정해야함
    PREV_LINK(heap_listp + 2 * WSIZE) = NULL;       //프롤로그 블록의 PREV
    PREV_LINK(heap_listp + 3 * WSIZE) = NULL;       //프롤로그 블록의 NEXT
    PUT(heap_listp + 4 * WSIZE, PACK(2 * DSIZE, 1));
    PUT(heap_listp + 5 * WSIZE, PACK(0, 1));

    linked_listp = heap_listp + 2 * WSIZE;          //가용블럭 연결리스트의 첫 가용블록의 시작주소를 가리키는 포인터

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }

    return 0;
}

void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1){
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    int test0=GET(HDRP(PREV_BLKP(bp)));
    int test1=GET(FTRP(PREV_BLKP(bp)));


    return coalesce(bp);
}

void *find_fit(size_t asize){      
    void *bp;

    for (bp = linked_listp; GET_ALLOC(HDRP(bp)) != 1; bp = NEXT_LINK(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}

void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    delete_linked(bp);

    if ((csize - asize) >= (3 * DSIZE)) {   //할당할 블록의 크기와 가용 블록의 크기가 3 * DSIZE 보다 클 때
        PUT(HDRP(bp), PACK(asize, 1));      //할당된 블록의 헤더와 풋터에 블록 크기와 할당 정보를 넣고
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));//남은 블록을 가용 블록으로 만들어 준 뒤 가용 블럭 연결 리스트에 추가시킨다.
        PUT(FTRP(bp), PACK(csize-asize, 0));
        insert_linked(bp);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void insert_linked(void *bp) {
    PREV_LINK(bp) = NULL;
    NEXT_LINK(bp) = linked_listp;
    PREV_LINK(linked_listp) = bp;

    linked_listp = bp;
}

void delete_linked(void *bp) {

    if (bp == linked_listp) {    // bp가 가용 연결 리스트의 첫 가용 블럭일 때  
        PREV_LINK(NEXT_LINK(bp)) = NULL;
        linked_listp = NEXT_LINK(bp);
    } else {
        NEXT_LINK(PREV_LINK(bp)) = NEXT_LINK(bp);
        PREV_LINK(NEXT_LINK(bp)) = PREV_LINK(bp);
    }
}

void *mm_malloc(size_t size) {
    char *bp;
    size_t asize;
    size_t extendsize;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 3 * DSIZE; //PREV와 NEXT를 추가로 넣어줬기 때문에 빈블록이 없어도 16바이트가 필요
    } else {
        asize = DSIZE * ((size + (2 * DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        insert_linked(bp);
    } else if (prev_alloc && !next_alloc) {
        delete_linked(NEXT_BLKP(bp));   //다음 블록을 가용 연결 리스트에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));   //헤더의 위치를 먼저 저장 해주어야 풋터의 위치를 알 수 있음
        PUT(FTRP(bp), PACK(size, 0));
        insert_linked(bp);
    } else if (!prev_alloc && next_alloc) { 
        delete_linked(PREV_BLKP(bp));   //이전 블록을 가용 연결 리스트에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_linked(bp);
    } else {
        delete_linked(PREV_BLKP(bp));   //이전 블록을 가용 연결 리스트에서 제거
        delete_linked(NEXT_BLKP(bp));   //다음 블록을 가용 연결 리스트에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_linked(bp);
    }

    return bp;
}

void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if (size < copySize) {
        copySize = size;
    }

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
