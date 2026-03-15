#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "smalloc.h"

#define PAGE_SIZE   4096  // mmap only allocs 4096
#define HEADER_SIZE 24 // header, payload, pad
#define ALIGNMENT   8

// rounds x up to the nearest multiple of a, e.g. align(10, 8) = 16
static int align(int x, int a) {
  return (x + a - 1) & ~(a - 1);
}

typedef struct Block {
  int block_size;
  int allocated; // 1 = in use, 0 = free
  struct Block *next_free; // next free block
  struct Block *prev_free; // previous free block
} Block;

static void *start_of_heap = NULL; // first byte of the mmap region
static Block *free_block = NULL; // head of the explicit free list

/*
 * my_init() is called one time by the application program to to perform any 
 * necessary initializations, such as allocating the initial heap area.
 * size_of_region is the number of bytes that you should request from the OS using
 * mmap().
 * Note that you need to round up this amount so that you request memory in 
 * units of the page size, which is defined as 4096 Bytes in this project.
 */
int my_init(int size_of_region) {
  /*
   * Implement your initialization here.
   */
  int rounded = align(size_of_region, PAGE_SIZE); // round up to nearest page boundary

  int fd = open("/dev/zero", O_RDWR); // /dev/zero gives zeroed memory when mmap'd
  if (fd < 0) return -1;

  void *mem = mmap(NULL, rounded, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  // notes for myself - null is let os choose, read/write
  close(fd);
  if (mem == MAP_FAILED) return -1;

  start_of_heap = mem;

  Block *b = (Block *)mem; // the entire region starts as one big free block
  b->block_size = rounded;
  b->allocated = 0;
  b->next_free = NULL;
  b->prev_free = NULL;

  free_block = b;
  return 0;
}

/*
 * get_head_pointer() returns an offset to the start of head block (not the payload) 
 * relative to the start of the heap.
 * This should be (unsigned long)head_pointer - (unsigned long)start_of_heap.
 * If there is no free block, return -1.
 */
int get_head_pointer(Pointer_Status* status) {
  /*
   * Implement your get_head_pointer here.
   */
  // if no free block, return fail
  if (free_block == NULL) {
    status->success = 2;
    status->block_size = -1;
    return -1;
  }
  // there's a free block
  status->success = 3;
  status->block_size = free_block->block_size;
  // byte offset starting from header of heap, not payload from spec
  return (int)((unsigned long)free_block - (unsigned long)start_of_heap);
}

/*
  * get_next_pointer() takes as input a pointer to the payload of a block
  * and returns an offset to the payload of the next free block in the 
  * free list (relative to the start of the heap).
  * If there is no next free block, return -1.
  * NOTE: "curr" points to the start of the payload of a block.
 */
int get_next_pointer(void *curr) {
  /*
   * Implement your get_next_pointer here.
   */
  Block *b = (Block *)((char *)curr - HEADER_SIZE);

  // if there is no next free block, then failed
  if (b->next_free == NULL) return -1;

  // the next free block's payload starts HEADER_SIZE bytes after header
  // sim to above, return offset from start of heap
  void *next_payload = (char *)b->next_free + HEADER_SIZE;
  return (int)((unsigned long)next_payload - (unsigned long)start_of_heap);
}

/*
  * get_prev_pointer() takes as input a pointer to the payload of a block
  * and returns an offset to the payload of the previous free block in the 
  * free list (relative to the start of the heap).
  * If there is no previous free block, return -1.
  * NOTE: "curr" points to the start of the payload of a block.
 */
int get_prev_pointer(void *curr) {
  /*
   * Implement your get_prev_pointer here.
   */
  // basically identical to get_next_pointer
  Block *b = (Block *)((char *)curr - HEADER_SIZE);

  if (b->prev_free == NULL) return -1;

  void *prev_payload = (char *)b->prev_free + HEADER_SIZE;
  return (int)((unsigned long)prev_payload - (unsigned long)start_of_heap);
}

/*
 * smalloc() takes as input the size in bytes of the payload to be allocated and 
 * returns a pointer to the start of the payload. The function returns NULL if 
 * there is not enough contiguous free space within the memory allocated 
 * by my_init() to satisfy this request.
 */
void *smalloc(int size_of_payload, Malloc_Status *status) {
  /*
   * Implement your malloc here.
   */
  // total bytes needed is header plus payload rounded up to 8-byte boundary
  /* int needed = align(HEADER_SIZE + size_of_payload, ALIGNMENT);*/
  int needed = align(HEADER_SIZE + size_of_payload, ALIGNMENT); // revert test change

  Block *cur = free_block; // start first-fit search from the head
  int hops = 0; // number of blocks checked before finding a fit - impl later

  while (cur != NULL) { // traverse instead of j checking one block
    if (cur->block_size >= needed) break;
    cur = cur->next_free;
    hops++;
  }

  if (cur == NULL) {
    status->success = 0;
    status->payload_offset = -1;
    status->hops = -1;
    return NULL;
  }

  int remainder = cur->block_size - needed; // leftover bytes after carving out needed

  // need to rem curr from the free list
  if (cur->prev_free) cur->prev_free->next_free = cur->next_free;
  else free_block = cur->next_free;
  if (cur->next_free) cur->next_free->prev_free = cur->prev_free;

  // can't rly figure out the err
  if (remainder >= HEADER_SIZE) { // test change 1
    // remainder fits a new block header, so split it off as a free block
    Block *split = (Block *)((char *)cur + needed); // split starts right after cur
    split->block_size = remainder;
    split->allocated = 0;

    split->next_free = cur->next_free; // inherit curr's next
    // since we insert right after curr's old spot
    split->prev_free = cur->prev_free;

    // instead of j inserting as head, change pointers
    if (split->next_free) {
      split->next_free->prev_free = split;
    }
    if (split->prev_free) {
      split->prev_free->next_free = split;
    }
    else {
      free_block = split;
    }

    cur->block_size = needed;
  }

  // if remainder < HEADER_SIZE it becomes padding

  cur->allocated = 1;
  cur->next_free = NULL;
  cur->prev_free = NULL;

  void *payload = (char *)cur + HEADER_SIZE; // payload starts right after the header

  status->success = 1;
  status->payload_offset = (int)((unsigned long)payload - (unsigned long)start_of_heap);
  status->hops = hops;
  return payload;
}


/*
 * sfree() frees the target block. "ptr" points to the start of the payload.
 * NOTE: "ptr" points to the start of the payload, rather than the block (header).
 */
void sfree(void *ptr)
{
  /*
   * Implement your free here.
   */
  // now need to address freed block
  if (ptr == NULL) return;

  Block *b = (Block *)((char *)ptr - HEADER_SIZE);
  b->allocated = 0;

  // insert b into free list in addy order
  if (free_block == NULL || b < free_block) {
    // addy is at start, so insert head
    b->next_free = free_block;
    b->prev_free = NULL;

    if (free_block) {
      free_block->prev_free = b;
    }
    free_block = b;
  }

  else {
    // find the block just before b in address order
    Block *prev = free_block;
    
    while (prev->next_free != NULL && prev->next_free < b) {
      prev = prev->next_free;
    } // put in curr pos, again change pointers
    
    b->next_free = prev->next_free;
    b->prev_free = prev;

    if (prev->next_free) {
      prev->next_free->prev_free = b;
    }
    prev->next_free = b;
  }

  // merge w next neighbor if adjacent, change pointers
  if (b->next_free != NULL && (char *)b + b->block_size == (char *)b->next_free) {
    Block *next = b->next_free;
    b->block_size += next->block_size;
    b->next_free = next->next_free;
    if (next->next_free) next->next_free->prev_free = b;
  }

  // merge w previous neighbor, same idea
  if (b->prev_free != NULL && (char *)b->prev_free + b->prev_free->block_size == (char *)b) {
    Block *prev = b->prev_free;
    prev->block_size += b->block_size;
    prev->next_free = b->next_free;
    if (b->next_free) b->next_free->prev_free = prev;
  }
}
