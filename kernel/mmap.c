#ifndef LABS_MMAP
#define LABS_MMAP
#endif 

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "fcntl.h"

#define max(a, b) ((a) > (b) ? (a) : (b))



void *do_mmap(void *addr, int length, int prot, int flags, int fd, int offset) {
  if (length < 0 || offset < 0 || offset % PGSIZE != 0) {
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
  struct vma *free_vma = 0; 
  for (int i = 0; i < VMA_SIZE; i++) {
    if (!p->vmas[i].start) {
      free_vma = &p->vmas[i];
      break;
    }
  }
  
  if (!free_vma) { // no free vma slot
    release(&p->lock);
    return (void *)-1; 
  } 

  uint64 start = MIN_VMA_ADDR; 
  for (int i = 0; i < VMA_SIZE; i++) {
    if (p->vmas[i].start) {
        start = max(start, p->vmas[i].start + p->vmas[i].length);
    }
  }

  if (start + length >= TRAPFRAME) { // no enough space for new vma
    release(&p->lock);
    return (void *)-1; 
  }

  start = PGROUNDUP(start);
  free_vma->start = (uint64) start;
  free_vma->length = length;
  free_vma->prot = prot;
  free_vma->flags = flags;
  free_vma->file = p->ofile[fd];
  free_vma->offset = offset;

  filedup(free_vma->file);
  release(&p->lock);
  return (void *) start;
}

struct vma *find_vma(struct proc *p, uint64 addr) {
  struct vma *vma; 
  for (int i = 0; i < VMA_SIZE; i++) {
    vma = &p->vmas[i];
    if (vma && vma->start <= addr && addr < vma->start + vma->length) {
      return &p->vmas[i];
    }
  }
  return 0;
}

int read_from_file(struct vma *vma, uint64 va, uint64 mem) {
  struct file *f = vma->file; 
  if (!f) {
    panic("vma has no file");
  }
  
  struct inode *ip = f->ip; 
  if (!ip) {
    panic("vma file has no inode");
  }

  uint64 offset = vma->offset + (va - vma->start);
  if (offset > ip->size) {
    panic("offset exceeds file size");
  }

  int n = offset + PGSIZE > ip->size ? ip->size - offset : PGSIZE;
  ilock(ip);
  if (readi(ip, 0, mem, offset, n) != n) {
    iunlock(ip);
    return -1;
  }
  iunlock(ip);
  return 0; 

}