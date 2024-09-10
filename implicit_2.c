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

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE   4           /*1워드 사이즈*/

#define DSIZE   8           /*2워드 사이즈*/

#define CHUNKSIZE   (1<<12) /*힙의 최대 크기*/

#define MAX(x, y) ((x) > (y)? (x) : (y))        /*최댓값 함수*/

#define PACK(size, alloc)   ((size) | (alloc))  /*블록의 크기와 할당 상태를 합쳐줌*/

#define GET(p)          (*(unsigned int *)(p))  /*블록의 정보를 읽을 때 사용.*/

#define PUT(p, val)     (*(unsigned int *)(p) = (val)) /*블록에 정보를 쓸 때 사용*/

#define GET_SIZE(p)     (GET(p) & ~0x7) /*하위 3비트를 제거하여 블록의 크기 정보만 추출 할 때 사용. 패딩을 사용해서 8의 배수로 만들어줌*/

#define GET_ALLOC(p)    (GET(p) & 0x1)  /*블록의 할당 상태를 나타낼 때 사용*/

#define HDRP(bp)        ((char *)(bp) - WSIZE)  /*현재 블록의 헤더 위치를 계산. 블록시작주소(bp)에서 한 워드(4바이트)를 빼서 헤더를 가리키는 역할을 해*/

#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /*현재 블록의 풋터 위치를 계산*/

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))   /*다음 블록의 시작 주소를 계산하는 매크로 함수*/

#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))   /*이전 블록의 시작 정보를 확인하거나 탐색할 때 사용*/

int mm_init(void)
{

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1) { //힙에 메모리를 추가할 수 없다면
        return -1;                                           //-1을 반환
    }

    PUT(heap_listp, 0);                                     //패딩 : 사용되지 않는 공간
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));          //프롤로그 헤더 : 힙의 제일 처음에 고정된 블록의 헤더(8바이트)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));          //프롤로그 풋터 : 힙의 제일 처음에 고정된 블록의 풋터(8바이트)
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));              //에필로그 풋터 : 크기가 0이고, 할당은 1이다. find함수를 할 때, 힙의 끝까지 탐색할 때 사용된다.
    heap_listp += (2 * WSIZE);                              //heap_listp는 프롤로그 헤더와 풋터를 지나, 프롤로그의 끝에 위치하게 됌.

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {             //CHUNKSIZE를 WSIZE로 나누면, 몇 블록을 확장할 수 있는지 알 수 있음
        return -1;                                          //-1을 반환
    }
    return 0;                                               //초기화가 되면, 0을 반환
}

void *extend_heap(size_t words) {               
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;   //메모리는 2워드(8바이트) 단위로 관리되므로, 홀수 워드일 경우 패딩을 추가해서 짝수 워드로 맞추는 과정
    if ((long)(bp = mem_sbrk(size)) == -1) {                    //만약 더이상 힙에 추가할 메모리가 없다면
        return NULL;                                            //NULL을 반환
    }

    PUT(HDRP(bp), PACK(size, 0));                               //확장된 블록의 헤더에 블록의 크기를 size로, 할당되지 않았음(0)을 나타냄
    PUT(FTRP(bp), PACK(size, 0));                               //확장된 블록의 풋터에 블록의 크기를 size로, 할당되지 않았음(0)을 나타냄
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));                       //다음 블록의 헤더에 0의 크기와 1의 상태를 추가

    return coalesce(bp);                                        //새로 확장된 블록을 인접한 가용 블록과 병합
}

void *find_fit(size_t asize) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) { //힙 안에 있는 모든 가용 블록을 탐색
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {    //만약 가용블록이고, 요구하는 사이즈가 가용블록 사이즈 보다 작다면
            return bp;                                                  //해당 가용 블록의 시작점을 반환
        }
    }
    return NULL;                                                        //가용 블록이 없으면 NULL반환
}

void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));                                  //가용블록의 크기

    if ((csize - asize) >= (2*DSIZE)) {                                 //가용블록의 크기와 할당하고자 하는 크기의 차이가 16바이트를 넘으면,
        PUT(HDRP(bp), PACK(asize, 1));                                  //할당할 블록에 할당하고, 남은 블록은 가용블록상태로 만들어줌
        PUT(FTRP(bp), PACK(asize, 1));                                  //할당된 블록의 헤더와 풋터에는 할당상태를 1로 바꿔줌
        bp = NEXT_BLKP(bp);                                             //다음 블록의 시작점으로 이동
        PUT(HDRP(bp), PACK(csize-asize, 0));                            //할당하고 남은 가용블록의 크기는 전체 가용블록에서 할당한 가용블록 크기를 뺸 값이다
        PUT(FTRP(bp), PACK(csize-asize, 0));                            //할당 상태는 0으로 설정
    } else {
        PUT(HDRP(bp), PACK(csize, 1));                                  //csize와 asize가 16바이트보다 차이가 안난다면,
        PUT(FTRP(bp), PACK(csize, 1));                                  //csize전체를 할당블록으로 설정
    }
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {            //할당할 size가 0 이면 NULL반환
        return NULL;
    }

    if (size <= DSIZE) {        //할당할 size가 8바이트(1워드)보다 작으면
        asize = 2 * DSIZE;      //asize에 최소 블록 크기인 16바이트(2워드)를 대입
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);   //asize의 size보다 큰 8의 배수로 맞춤
    }

    if ((bp = find_fit(asize)) != NULL) {                           //만약 asize 크기의 가용블록이 존재하면
        place(bp, asize);                                           //해당 블록의 상태를 할당된 블록으로 변경
        return bp;                                                  //해당 블록의 시작 주소를 반환
    }

    extendsize = MAX(asize, CHUNKSIZE);                             //확장시킬 사이즈는 asize와 CHUNKSIZE의 최댓값이다
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {             //일단, extend_heap으로 힙 확장. 만약 extendsize만큼 확장시킬 수 없다면
        return NULL;                                                //NULL을 반환
    }
    place(bp, asize);                                               //확장된 블록을 할당된 블록으로 변경
    return bp;                                                      //할당된 블록의 시작 주소를 반환
}

void mm_free(void *ptr)                                             //해제할 블록의 시작점포인터를 매개변수로 설정
{
    size_t size = GET_SIZE(HDRP(ptr));                              //할당된 블록을 해제할때는, 해제할 블록의 사이즈를 알아야함

    PUT(HDRP(ptr), PACK(size, 0));                                  //헤더의 할당상태를 0으로 바꿔줌
    PUT(FTRP(ptr), PACK(size, 0));                                  //풋터의 할당상태를 0으로 바꿔줌
    coalesce(ptr);                                                  //해제된 가용블록을 다른 가용블록과 연결시켜줌
}

void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));         //이전 블록의 할당상태를 나타냄
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));         //다음 블록의 할당상태를 나타냄
    size_t size = GET_SIZE(HDRP(bp));                           //현재 블록의 크기를 size에 저장

    if (prev_alloc && next_alloc) {                             //이전 블록과 다음 블록이 모두 할당상태라면
        return bp;                                              //현재 블록을 그대로 반환
    } else if (prev_alloc && !next_alloc) {                     //다음 블록이 가용상태라면,
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));                  //size에 다음 블록의 크기를 합쳐주고
        PUT(HDRP(bp), PACK(size, 0));                           //병합된 블록의 헤더에 새 크기를 저장(가용 상태로 변경)
        PUT(FTRP(bp), PACK(size, 0));                           //병합된 블록의 풋터에 새 크기를 저장
    } else if (!prev_alloc && next_alloc) {                     //만약 이전 블록이 가용블록이고, 다음블록은 할당상태라면
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));                  //이전 블록의 크기를 현재 블록의 크기에 더함
        PUT(FTRP(bp), PACK(size, 0));                           //병합된 블록의 풋터에 새 크기를 저장
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                //병합된 블록의 헤더에 새 크기를 저장(이전 블록 헤더)
        bp = PREV_BLKP(bp);                                     //bp를 이전 블록의 시작 주소로 갱신(병합도니 블록의 시작 주소)
    } else {                                                    //이전 블록과 다음 블록 모두 가용상태라면
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  //이전, 다음 블록 크기를 현재 블록에 더함
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                //병합된 블록의 헤더에 새 크기를 저장(이전 블록의 헤더)
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));                //병합된 블록읲 풋터에 새 크기를 저장(다음 블록의 풋터)
        bp = PREV_BLKP(bp);                                     //bp를 병합된 블록의 시작 주소로 갱신
    }

    return bp;                                                  //병합된 블록의 시작 주소를 반환
}

void *mm_realloc(void *ptr, size_t size)                        //기존 블록의 크기를 변경하는 함수(크기 조정)
{
    void *oldptr = ptr;                                         //기존 블록의 시작 주소를 oldptr에 저장
    void *newptr;                                                       
    size_t copySize;                                            //기존 블록에서 복사할 데이터 크기를 저장할 변수
    
    newptr = mm_malloc(size);                                   //새로운 크기(size)를 할당받고, 그 시작 주소를 newptr에 저장
    if (newptr == NULL)                                         //만약 새로운 메모리 할당에 실패하면       
      return NULL;                                              //NULL을 반환하여 실패를 알림                  
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;                  //기존 블록의 크기에서 헤더와 풋터 크기(DSIZE, 8바이트)를 뺸 데이터 크기를 저장 
    if (size < copySize)                                        //새로운 크기가 기존 데이터보다 작다면                        
      copySize = size;                                          //새로운 크기만큼만 복사하도록 copySize를 줄임                          
    memcpy(newptr, oldptr, copySize);                           //기존 데이터의 copySize만큼을 새로운 블록(newptr)으로 복사    
    mm_free(oldptr);                                            //기본 블록을 해제하여 가용 블록으로 변경        
    return newptr;                                              //새로운 블록의 시작 주소를 반환                   
}