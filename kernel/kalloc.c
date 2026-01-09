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


#define PG_REF_IDX(pa) (((uint64) pa - KERNBASE) / PGSIZE) 
#define MAX_PG_REF_IDX PG_REF_IDX(PHYSTOP)
int pg_ref_cnt[MAX_PG_REF_IDX + 1]; 

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    pg_ref_cnt[PG_REF_IDX(p)] = 1; // initialize reference count to one
    kfree(p);
  }
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
  
  acquire(&kmem.lock);
  if (pg_ref_cnt[PG_REF_IDX(pa)] < 1) {
    release(&kmem.lock);
    panic("kfree: reference count is invalid");
  }

  if (--pg_ref_cnt[PG_REF_IDX(pa)] == 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  
  release(&kmem.lock);
}

void krefence_dec(void *pa) 
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP )
    panic("kfree");


  acquire(&kmem.lock);
  if (pg_ref_cnt[PG_REF_IDX(pa)] < 2) {
    panic("krefence_dec: reference count is invalid");
  }
  pg_ref_cnt[PG_REF_IDX(pa)]--;
  release(&kmem.lock);
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
    if (pg_ref_cnt[PG_REF_IDX(r)] != 0) {
    panic("kalloc: reference count not zero on free list");
    }
    kmem.freelist = r->next;
    pg_ref_cnt[PG_REF_IDX(r)] = 1;
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
  
  acquire(&kmem.lock);
  if (pg_ref_cnt[PG_REF_IDX(pa)] < 1) {
    panic("krefence_inc: reference count is invalid");
  }
  pg_ref_cnt[PG_REF_IDX(pa)]++;
  release(&kmem.lock);
}