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
  int state; // state > 0: number of readers holding the lock; state == 0: lock is free; state == -1: a writer is holding the lock
  uint waiting_writer; // Number of writers waiting to acquire the lock
};
#endif
