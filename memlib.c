/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  /* points to first byte of heap 가상 힙의 첫 번째 바이트를 가리키는 포인터*/
static char *mem_brk;        /* points to last byte of heap 가상 힙에서 현재 사용 중인 마지막 바이트를 가리키는 포인터*/
                             /* brk는 시스템의 brk 함수처럼 힙의 끝을 가리키는 개념*/
static char *mem_max_addr;   /* largest legal heap address 가상 힙에서 가장 큰 주소(즉, 최대 힙 크기)*/ 

/* 
 * mem_init - initialize the memory system model
 * 메모리 시스템을 초기화 하는 함수. 즉, 사용할 가상 메모리를 할당
 */
void mem_init(void)
{
    /* allocate the storage we will use to model the available VM */
    /*MAX_HEAP은 config.h에 정의되어 있고, 그 크기는 20MB이다*/
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
	fprintf(stderr, "mem_init_vm: malloc error\n");
	exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address 힙의 최대 크기를 MAX_HEAP으로 설정하고, 이를 위한 메모리를 동적으로 할당해*/
    mem_brk = mem_start_brk;                  /* heap is empty initially 초기에는 힙이 비어 있으니까 mem_brk는 힙의 시작 주소와 같다.*/
}

/* 
 * mem_deinit - free the storage used by the memory system model
 * 할당된 메모리를 해제하는 함수. mem_init에서 할당한 메모리를 모두 해제해서 나중에 메모리 누수를 방지한다.
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 * 가상 힙의 brk 포인터를 다시 힙의 시작점으로 되돌려서 힙을 비운 상태로 리셋
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 * 실제 sbrk 시스템 호출과 같은 기능을 하는 함수. 이 함수는 힙을 늘리는 역할을 함
 * incr 만큼 힙을 확장해서 그 확장된 부분의 시작 주소를 반환. 하지만 힙을 줄이는건 허용하지 않음
 * 만약 확장하려는 크기가 너무 크면 메모리가 부족하다는 에러를 반환(ENOMEM)
 */
void *mem_sbrk(int incr) 
{
    char *old_brk = mem_brk;

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM;
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
	return (void *)-1;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
 * 가상 힙의 첫 번째 바이트의 주소를 반환
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
 * 가상 힙에서 현재 사용 중인 마지막 바이트의 주소를 반환
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 * 가상 힙의 크기를 반환. 이는 현재 mem_brk가 가리키는 주소에서 시작 주소를 뺸 값
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 * 현재 시스템의 페이지 크기를 반환. 이건 시스템 콜인 getpagesize()를 이용해서 반환.
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
