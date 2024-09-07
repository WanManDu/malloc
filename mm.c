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

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static char *heap_listp;

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "4team",
    /* First member's full name */
    "hyeda",
    /* First member's email address */
    "hyeda@gmail.com",
    /* Second member's full name (leave blank if none) */
    "eunsang",
    /* Second member's email address (leave blank if none) */
    "eunsang@gmail.com"
};

/* single word (4) or double word (8) alignment */
/* 메모리 정렬을 위해 사용되는 상수*/
/*8바이트 정렬을 의미함*/
/*할당되는 메모리 블록이 8의 배수로 정렬되도록 설정*/
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
/*주어진 size값을 8바이트의 배수로 반올림하는 매크로 함수*/
/*만약 14바이트를 할당하려고 하면, ALIGN(size)를 사용해 16바이트로 올림하여 할당*/
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/*시스템에서 사용하는 size_t 타입의 크기를 8바이트로 정렬한 값*/
/*size_t는 메모리 할당 시 사용되는 기본적인 크기 타입*/
/*메모리 할당을 할 때 size_t 크기만큼의 공간을 할당하거나 크기를 계산할 때 사용돼*/
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* Basic constants and macros */
/*메모리에서 한 워드의 크기를 정의하는 상수*/
/*여기서는 헤더와 풋터의 크기와 동일*/
/*메모리 블록의 헤더와 풋터를 관리할 때, 워드 단위로 크기를 계산하거나 블록 간의 크기 계산에 사용돼*/
#define WSIZE   4           /* Word and header/footer size (bytes) */

/*2워드, 즉 8바이트 크기를 정의하는 상수*/
/*메모리 할당 시 두 배수로 크기를 정렬할 때 사용됨*/
/*블록 간의 계산이나 메모리 블록을 8바이트로 정렬할 때 활용 됨*/
#define DSIZE   8           /* Double word size (bytes) */

/*힙을 확장할 때, 한 번에 확장할 기본 크기로, 여기서는 4kb(1<<12)로 설정 되어 있음*/
/*프로그램이 메모리를 더 많이 요구할 때 힙을 확장하는데, sbrk나 mem_sbrk 함수로 힙을 늘릴 때 기본적으로 이만큼의 크기를 한번에 확장*/
/*작은 크기로 여러번 확장하면 성능이 떨어질 수 있기 때문에 일정 크기만큼 확장하는 것이다.*/
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount (bytes) */

/*두 값중 더 큰값을 반환*/
/*요청한 메모리 크기가 너무 작을 경우, 최소 할당 단위로 강제해야 할 때 사용*/
#define MAX(x, y) ((x) > (y)? (x) : (y))

/*Pack a size and allocated bit into a word*/
/*메모리 블록의 크기와 할당여부(1/0)를 하나의 워드로 묶는 매크로 함수*/
/*크기는 상위 비트에, 할당 여부는 하위비트에 저장*/
/*메모리 블록의 헤더와 풋터에 크기와 할당 상태를 저장할 때 사용*/
#define PACK(size, alloc)   ((size) | (alloc))

/*Read and write a word at address p */
/*주어진 주소 p에서 값을 읽거나, 값을 써주는 매크로 함수*/
/*메모리 블록의 헤더와 풋터에 값을 읽을 때 사용*/
/*블록의 할당 상태나 크기를 가져오거나 업데이트 할 때 필요함*/
#define GET(p)          (*(unsigned int *)(p))

/*메모리 블록의 헤더와 풋터에 값을 쓸 때 사용*/
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/*Read the size and allocated fields from address p */
/*주어진 포인터 p가 가리키는 헤더나 풋터에서 블록의 크기를 추출하는 매크로 함수*/
/*마지막 3비트는 할당 상태 정보이므로 이를 제외하기 위해 ~0x7마스크를 사용*/
/*메모리 블록의 헤더나 풋터에서 블록 크기를 읽어올 때 사용*/
/*블록 크기를 확인해 다음 블록의 위치를 계산할 때 필요함*/
#define GET_SIZE(p)     (GET(p) & ~0x7)

/*주어진 포인터 p에서 블록의 할당 상태(1:할당됨, 0:비어있음)를 확인하는 매크로 함수*/
/*블록이 할당되었는지 확인할 때 사용.*/
/*메모리 블록을 관리할 때, 블록이 비어있는지 할당된 상태인지 확인하는 것이 중요함*/
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
/*주어진 블록 포인터 bp로부터 해당 블록의 헤더 주소를 계산하는 매크로 함수*/
/*메모리 블록을 관리할 때, 블록 포인터로부터 헤더에 접근해 블록의 크기나 할당 상태를 읽어오거나 수정할 때 사용*/
#define HDRP(bp)        ((char *)(bp) - WSIZE)

/*주어진 블록 포인터 bp로부터 해당 블록의 풋터 주소를 계산하는 매크로 함수*/
/*블록의 크기와 할당 상태는 풋터에도 저장되어있음*/
/*풋터를 사용해 블록의 정보를 읽거나 수정할 때 사용*/
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
/* 주어진 블록 포인터 bp로부터 다음 블록의 주소를 계산하는 매크로 함수*/
/* 메모리 블록을 순차적으로 탐색할 때, 현재 블록 포인터에서 다음 블록의 위치를 계산할 때 사용 됨*/
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))

/* 주어진 블록 포인터 bp로부터 이전 블록의 주소를 계산하는 매크로 함수*/
/* 이전 블록의 정보를 확인하거나 탐색할 때 사용.*/
/* 블록을 병합할 때 이전 블록을 확인하는 데도 쓰일 수 있다.*/
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 
 * mm_init - initialize the malloc package.
 * malloc 패기지를 초기화 하는 함수
 * 메모리 시스템을 설정하고 힙을 준비
 * 초기 힙을 만들고, 초기화된 힙을 확장하는 작업을 수행
 */
int mm_init(void)
{
    /*create the initial empty heap*
    * 4개의 워드 크기(16바이트)만큼 확장, 초기 힙을 만듦*/
    
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1) { /*(void *) - 1의 의미는 유효하지 않은 포인터나 에러를 나타내기 위한 값으로 사용*/
        return -1;
    }

    PUT(heap_listp, 0); //정렬을 위한 padding. 정렬을 맞추기 위해 0 저장. 실제 블록으로는 사용되지 않음
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); //프롤로그 헤더를 설정. 블록의 크기는 DSIZE이고, 할당되었다는 의미에서 1을 더함
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); //프롤로그 풋터를 설정. 헤더와 마찬가지로 8바이트 크기로 설정
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      //에필로그 헤더. 크기가 0인 할당된 블록을 나타내며, 힙의 끝을
    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes*/
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/*
* 힙을 확장하고, 확장된 메모리 블록을 가용 블록으로 설정
* 힙이 새로운 가용 블록으로 확장. 
* 이때 가용블록의 헤더와 풋터가 설정. 
* 확장된 블록 끝에 새로운 에필로그 블록이 추가
* 힙에 충분한 공간이 없을 때 호출되어 새로운 블록을 추가
* 확장된 블록은 'coalesce'를 통해 인접한 가용 블록과 병합될 수 있다.
*/
void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/* 
* 요청된 크기만큼의 가용 블록을 찾는 함수
* 힙에서 탐색이 이루어지지만, 블록 상태는 변하지 않음
* mm_malloc에서 호출되어 가용 블록이 있는지 확인
* 적절한 가용 블록을 찾으면 그 블록의 주소를 반환
*/
void *find_fit(size_t asize) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

/*
* 찾은 가용 블록을 할당하고, 필요하면 블록을 분할
* 블록이 할당되면, 해당 블록의 헤더와 풋터가 할당된 상태로 업데이트 된다
* 블록이 너무 크면, 남은 공간을 새로운 가용 블록으로 분할하여 가용 리스트에 추가
* find_fit 또는 extend_heap에서 적절한 블록을 찾으면, 이 함수가 호출되어 해당 블록을 할당
*/
void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 주어진 크기만큼의 메모리를 할당
 * 힙에서 적절한 크기의 가용 블록이 찾고, 없으면 힙을 확장
 * 블록이 할당된 후에는 헤더와 풋터가 업데이트되고, 가용 상태에서 할당 상태로 바뀜
 * find_fit을 사용해 적절한 블록을 찾고, place를 사용해 블록을 할당함
 * 적절한 블록이 없다면, extend_heap을 호출해 힙을 확장
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); //요청된 크기를 8바이트 배수로 반올림해서 정렬하는 방식으로 결정된다.
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

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) // bp == ptr
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0)); //가용 상태를 표시할때, PACK(size, 0)이면 가용상태로 돌아온것을 뜻한다.
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
* 인접한 가용 블록을 병합하여 더 큰 가용 블록을 만듬
* 인접한 가용 블록이 병합되어 하나의 큰 블록으로 합쳐짐
* 병합된 블록의 헤더와 풋터가 새로운 크기로 업데이트 됌
* mm_free에서 블록이 해제될 때 호출됨.
*extend_heap에서 힙이 확장된 후 호출되어 가용 블록으로 병합
*/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // 현재 블록을 다른 블록과 병합한 경우에, 앞 블록 또는 현재 블록 시작 주소로 포인터를 업데이트
    }

    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * 이미 할당된 메모리 블록의 크기를 조정하는 함수
 * 새로운 블록이 할당되고, 기존 블록의 데이터가 복사된 후, 기존 블록이 해제
 * mm_malloc을 통해 새로운 블록을 할당하고, memcpy로 데이터를 복사한 후
 * mm_free로 기존 블록을 해제함
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size); //새로 할당된 블록의 헤더와 풋터의 정보는 여기서 설정된다.
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














