// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct spinlock page_refrence_lock; 

#define PHYSICAL_PAGE_NUMBER (128 * 1024 / 4) 
short physcial_page_references[PHYSICAL_PAGE_NUMBER]; 

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_refrence_lock, "page_ref_lock");
  freerange(end, (void*)PHYSTOP);
  for (int i = 0; i < PHYSICAL_PAGE_NUMBER; i++) {
    physcial_page_references[i] = 0;
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP )
    panic("kfree");
  
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;

  acquire(&kmem.lock);
  acquire(&page_refrence_lock);
  int index = ((uint64) pa / PGSIZE); 
  int reference_count = physcial_page_references[index]; 
  if (reference_count < 0) {
    release(&page_refrence_lock);
    release(&kmem.lock);
    panic("kfree: reference count is negative");
  }

  if (reference_count > 1) {
    // there are other references, just decrease the count
    physcial_page_references[index] -= 1;
  } else {
    // no other references, free the page
    r->next = kmem.freelist;
    kmem.freelist = r;
    physcial_page_references[index] = 0;
  }
  release(&page_refrence_lock);
  release(&kmem.lock);
}

void krefence_dec(void *pa) 
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP )
    panic("kfree");
  
  int index = ((uint64) pa / PGSIZE); 
  int reference_count = physcial_page_references[index];
  if (reference_count < 1) {
    panic("kfree: reference count can not be negative");
  }

  acquire(&page_refrence_lock);
  physcial_page_references[index] -= 1;
  release(&page_refrence_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
     acquire(&page_refrence_lock);
     physcial_page_references[((uint64) r / PGSIZE)] = 1;
     release(&page_refrence_lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void krefence_inc(void *pa) 
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP )
    panic("kfree");
  
  int index = ((uint64) pa / PGSIZE); 
  int reference_count = physcial_page_references[index];
  if (reference_count < 0) {
    panic("kfree: reference count can not be negative");
  }

  acquire(&page_refrence_lock);
  physcial_page_references[index] += 1;
  release(&page_refrence_lock);
}