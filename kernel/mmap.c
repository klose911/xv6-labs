#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "fcntl.h"

static int prot2perm(int flags) {
  int perm = 0;
  if (flags & PROT_READ) {
    perm |= PTE_R;
  }
  
  if (flags & PROT_WRITE) {
    perm |= PTE_W;
  }
  return perm | PTE_U;
} 

void *do_mmap(void *addr, int length, int prot, int flags, int fd, int offset) {
  if (length == 0) {
    return (void *) -1; 
  } 

  struct proc *p = myproc();
  if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0) {
    return (void *) -1; 
  }
   
  if (prot & PROT_WRITE) {
    if (p->ofile[fd]->writable == 0) {
      return (void *)-1; 
    }
  } 

  if (prot & PROT_EXEC) {
    return (void *)-1; 
  } 

  if ((flags & MAP_SHARED) == 0 && (flags & MAP_PRIVATE) == 0) {
    return (void *)-1; 
  } 
  
  acquire(&p->lock);
  addr = (void *)uvmalloc(p->pagetable, p->sz, length, prot2perm(prot));
  if (addr == 0) {
    release(&p->lock);
    return (void *)-1; 
  } 

  struct vma *free_vma = 0; 
  for (int i = 0; i < VMA_SIZE; i++) {
    if (p->vmas[i].start == 0) {
      free_vma = &p->vmas[i];
      break;
    }
  }
  
  if (!free_vma) {
    release(&p->lock);
    return (void *)-1; 
  } 

  free_vma->start = (uint64) addr;
  free_vma->end = (uint64) addr + length;
  free_vma->prot = prot;
  free_vma->flags = flags;
  free_vma->file = p->ofile[fd];
  free_vma->offset = offset;

  filedup(free_vma->file);
  release(&p->lock);
  return addr;
}
