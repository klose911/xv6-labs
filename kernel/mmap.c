#ifndef LABS_MMAP
#define LABS_MMAP
#endif 

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "fcntl.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b)) 
#define MAX_LOG_SIZE (((MAXOPBLOCKS-1-1-2) / 2) * BSIZE)



void *do_mmap(void *addr, int length, int prot, int flags, int fd, int offset) 
{
  if (length < 0 || offset < 0 || offset % PGSIZE != 0) {
    return (void *) -1; 
  } 

  struct proc *p = myproc();
  if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0) {
    return (void *) -1; 
  }
   
  if ((prot & PROT_WRITE) && (flags & MAP_SHARED)) {
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

  struct vma *free_vma = 0; 
  for (int i = 0; i < VMA_SIZE; i++) {
    if (!p->vmas[i].start) {
      free_vma = &p->vmas[i];
      break;
    }
  }
  
  if (!free_vma) { // no free vma slot
    return (void *)-1; 
  } 

  uint64 start = MIN_VMA_ADDR; 
  for (int i = 0; i < VMA_SIZE; i++) {
    if (p->vmas[i].start) {
        start = max(start, p->vmas[i].start + p->vmas[i].length);
    }
  }

  if (start + length >= TRAPFRAME) { // no enough space for new vma
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
  return (void *) start;
}

struct vma *find_vma(struct proc *p, uint64 addr) 
{
  struct vma *vma; 
  for (int i = 0; i < VMA_SIZE; i++) {
    vma = &p->vmas[i];
    if (vma && vma->start <= addr && addr < vma->start + vma->length) {
      return &p->vmas[i];
    }
  }
  return 0;
}

int read_from_file(struct vma *vma, uint64 va, uint64 mem) 
{
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

static int write_back_to_file(struct vma *vma, uint64 va, int n);

int do_munmap(uint64 addr, int length) 
{
  if (length == 0 || addr % PGSIZE != 0 || 
  addr < MIN_VMA_ADDR || addr >= TRAPFRAME) {
    return -1; 
  }

  struct proc *p = myproc();
  struct vma *vma = find_vma(p, addr); 
  if (!vma) {
    return -1;  
  }

  if (vma->flags & MAP_SHARED) {
    uint64 va = addr; 
    uint64 end = min(length, vma->file->ip->size - vma->offset - (va - vma->start));
    while(va < addr + end) {
      if (isdirty(p->pagetable, va)) {
        // write back to file if the page is dirty
        if (write_back_to_file(vma, va, min(PGSIZE, addr + end - va)) < 0) {
          return -1; 
        }
      }
      va += PGSIZE;
    }
  } 

  // unmap the pages 
  uvmunmap(p->pagetable, addr, (length - 1) / PGSIZE + 1, 1);
  // update the vma slot
  if (addr == vma->start) {
    if (length == vma->length) {
      vma->start = 0;
      vma->length = 0;
      vma->prot = 0;
      vma->flags = 0;
      vma->offset = 0;
      fileclose(vma->file);
      vma->file = 0;
    } else {
      vma->start += length;
      vma->length -= length;
      vma->offset += length;
    }
  } else if (addr + length == vma->start + vma->length) {
    vma->length -= length;
  } else {
    panic("munmap can only unmap from the start or the end of the vma");
  }
  return 0;
}

int write_back_to_file(struct vma *vma, uint64 va, int n) 
{
    int i = 0;
    int r;
    struct file *f = vma->file;
    printf("write_back_to_file: va=0x%lx, n=%d\n", va, n);

    while(i < n){
      int n1 = n - i;
      if(n1 > MAX_LOG_SIZE)
        n1 = MAX_LOG_SIZE;

      begin_op();
      ilock(f->ip);

      if ((r = writei(f->ip, 1, va + i, vma->offset + (va - vma->start) + i, n1)) < n1) {
        iunlock(f->ip);
        end_op();
        break;
      }
      iunlock(f->ip);
      end_op();
      i += n1;
    }

    return i == n ? n : -1;
}