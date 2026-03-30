// Mutual exclusion lock.
#include "types.h"

struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts; // Number of calls to test-and-set.
  int n; // Number of calls to acquire().
#endif
};

#ifdef LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  // Replace this with your implementation.

  // condition:
  // 1. read acquire: writer == 0 && waiting_writer == 0
  // 2. write acquire: reader == 0 && writer == 0
  // 3. read release: reader > 0; 
  // 4. writer realse: writer > 0;  
  // uint reader;
  // uint writer;
  // struct spinlock l;
  int state; 
  uint waiting_writer;
};
#endif
