// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

#ifdef LAB_LOCK
#define NLOCK 500

// For testing: track the number of calls to acquire() 
//and test-and-set for each lock.
// locks数组存储系统中所有的锁，最多支持NLOCK个锁
static struct spinlock *locks[NLOCK];
// 保护locks数组的锁  
struct spinlock lock_locks; 

void
freelock(struct spinlock *lk)
{
  acquire(&lock_locks); // 获取lock_locks锁，保护locks数组
  int i;
  for (i = 0; i < NLOCK; i++) {
    if(locks[i] == lk) { // 找到要释放的锁
      locks[i] = 0; // 将该锁从locks数组中移除
      break;
    }
  }
  release(&lock_locks); // 释放lock_locks锁
}

static void
findslot(struct spinlock *lk) {
  acquire(&lock_locks); // 获取lock_locks锁，保护locks数组
  int i;
  for (i = 0; i < NLOCK; i++) { // 在locks数组中找到一个空位来存储新锁
    if(locks[i] == 0) { // 找到空位
      locks[i] = lk; // 将新锁存储在locks数组中
      release(&lock_locks); // 释放lock_locks锁
      return;
    }
  }
  panic("findslot");
}
#endif

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name; 
  lk->locked = 0;
  lk->cpu = 0;
#ifdef LAB_LOCK
  lk->nts = 0; // 初始化test-and-set调用次数为0
  lk->n = 0; // 初始化acquire调用次数为0
  findslot(lk); // 将新锁添加到locks数组中
#endif  
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

#ifdef LAB_LOCK
    __sync_fetch_and_add(&(lk->n), 1); // 原子地将acquire调用次数加1
#endif      

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
#ifdef LAB_LOCK
    __sync_fetch_and_add(&(lk->nts), 1);
#else
   ;
#endif
  }

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

#ifdef LAB_LOCK
static void
read_acquire_inner(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  while (1) {
    acquire(&rwlk->l);

    if (rwlk->writer == 0 && rwlk->waiting_writer == 0) {
        rwlk->reader++;
        release(&rwlk->l);
        break;
    }

    release(&rwlk->l);
  }
}

static void
read_release_inner(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  acquire(&rwlk->l);
  if (rwlk->reader <= 0) 
    panic("read release inner");
  
  rwlk->reader--;
  release(&rwlk->l);
}

static void
write_acquire_inner(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  acquire(&rwlk->l);
  rwlk->waiting_writer++;
  release(&rwlk->l);

  while (1) {
    acquire(&rwlk->l);

    if (rwlk->writer == 0 && rwlk->reader == 0) {
      rwlk->writer = 1;
      rwlk->waiting_writer--;
      release(&rwlk->l);
      break;
    }

    release(&rwlk->l);
  }
}

static void
write_release_inner(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  acquire(&rwlk->l);
  if (rwlk->writer <= 0) 
    panic("write release inner");
  rwlk->writer = 0;
  release(&rwlk->l);
}

void
read_acquire(struct rwspinlock *rwlk)
{
  push_off(); // disable interrupts to avoid deadlock.
  read_acquire_inner(rwlk);
}

void
read_release(struct rwspinlock *rwlk)
{
  read_release_inner(rwlk);
  pop_off();
}

void
write_acquire(struct rwspinlock *rwlk)
{
  push_off(); // disable interrupts to avoid deadlock.
  write_acquire_inner(rwlk);
}

void
write_release(struct rwspinlock *rwlk)
{
  write_release_inner(rwlk);
  pop_off();
}

void
initrwlock(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  rwlk->reader = 0;
  rwlk->writer = 0;
  rwlk->waiting_writer = 0;   
  initlock(&rwlk->l, "rwlk");
}

// Test rwspinlock implementation.
// 读写锁测试函数，验证读写锁的正确性和性能
static void
rwspinlock_test_step(uint step, const char *msg)
{
  // 用来同步测试步骤的barrier，确保所有CPU在同一步骤完成之前不会继续执行后续步骤
  static uint barrier; // Used to synchronize test steps across CPUs.
  // ncpu是等待barrier的CPU数量，应该与mp.c中的CPU数量相匹配
  const uint ncpu = 4; // Number of CPUs to wait for at the barrier.  Should match the number of CPUs in mp.c.

  // Atomically increment the barrier and then spin until all CPUs have incremented it
  // to synchronize test steps across CPUs.
  // 原子地将barrier加1，并且自旋等待直到所有CPU都将barrier加到ncpu * step
  // 确保所有CPU在同一步骤完成之前不会继续执行后续步骤
  __atomic_fetch_add(&barrier, 1, __ATOMIC_ACQ_REL);
  // 自旋等待直到所有CPU都将barrier加到ncpu * step，确保所有CPU在同一步骤完成之前不会继续执行后续步骤
  while (__atomic_load_n(&barrier, __ATOMIC_RELAXED) < ncpu * step) {
    // spin
  }

  // 只有CPU 0负责打印测试步骤的信息，避免多个CPU同时打印导致输出混乱
  if (cpuid() == 0) { // Only print from one CPU to avoid jumbled output.
    printf("rwspinlock_test: step %d: %s\n", step, msg);
  }
}

// 延迟函数
static uint
delay()
{
  static uint v;
  // Simple delay function that takes a while to run
  // to increase the chance of triggering bugs.
  // 一个简单的延迟函数，运行一段时间以增加触发错误的机会 
  for (int i = 0; i < 10000; i++) { 
    // 原子地将v加1，增加触发错误的机会
    __atomic_fetch_add(&v, 1, __ATOMIC_RELAXED); 
  }
  // Return the value to prevent the compiler from optimizing away the loop.
  // 返回v的值，防止编译器优化掉这个循环
  return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

uint64
sys_rwlktest()
{
  int r = 0;
  int step = 0;

  push_off(); // 关闭中断，避免在测试过程中发生上下文切换导致死锁 
  int id = cpuid(); // 获取当前CPU的ID，用于区分不同CPU在测试中的行为

  rwspinlock_test_step(++step, "initrwlock"); // 初始化读写锁

  static struct rwspinlock l; // 定义一个全局的读写锁，供测试使用
  if (id == 0) {
    initrwlock(&l); // 只有CPU 0负责初始化读写锁，避免多个CPU同时初始化导致竞争条件
  }

  // 测试多个CPU同时获取读锁的情况
  rwspinlock_test_step(++step, "concurrent read_acquire"); 

  for (int i = 0; i < 1000000; i++) // 大量调用read_acquire来增加触发竞争条件的概率
    read_acquire(&l); // 获取读锁

  // 测试多个CPU同时释放读锁的情况
  rwspinlock_test_step(++step, "concurrent read_release"); 

  for (int i = 0; i < 1000000; i++) // 大量调用read_release来增加触发竞争条件的概率
    read_release(&l); // 释放读锁

  // 为写者优先测试做准备
  rwspinlock_test_step(++step, "prepare read_acquire for writer priority test"); 

  if (id == 1) { // CPU 1负责获取读锁，为后续的写者优先测试做准备
    for (int i = 0; i < 30; i++) { // 获取多个读锁
      read_acquire(&l); // 获取读锁
    }
  }

  // 测试写者优先的情况，验证等待的写者是否能够优先获取锁
  rwspinlock_test_step(++step, "writer priority test"); 

  static uint flag;
  // CPU 0负责获取写锁，并设置flag，验证等待的读者是否能够在写者释放锁后获取到最新的flag值
  if (id == 0) { 
    write_acquire(&l); // 获取写锁
    __atomic_store_n(&flag, 1, __ATOMIC_RELAXED); // 设置flag，表示写锁已经被获取
    write_release(&l); // 释放写锁
  }

  if (id == 1) { // CPU 1负责释放之前获取的读锁，为后续的写者优先测试做准备
    delay(); // 延迟一段时间
    for (int i = 0; i < 10; i++) { // 释放部分读锁
      read_release(&l); // 释放读锁
    }
    delay(); // 延迟一段时间
    for (int i = 0; i < 10; i++) { // 释放剩余的读锁
      read_release(&l); // 释放读锁
    }
    delay(); // 延迟一段时间
    for (int i = 0; i < 10; i++) { // 释放剩余的读锁
      read_release(&l); // 释放读锁
    }
  }

  // CPU 2负责获取读锁，验证是否能够在写者释放锁后获取到最新的flag值
  if (id == 2) { 
    delay(); // 延迟一段时间
    read_acquire(&l); // 获取读锁
    uint f = __atomic_load_n(&flag, __ATOMIC_RELAXED); // 读取flag的值，验证是否能够获取到最新的值
    if (f == 0) { // 如果flag的值为0，说明读者在写者释放锁之前获取了锁，违反了写者优先的原则
      printf("rwspinlock_test: reader sneaked ahead of waiting writer\n"); 
      r = -1;
    }
    read_release(&l); // 释放读锁
  }

  // 检查是否存在并发的读者和写者，验证读写锁的正确性
  rwspinlock_test_step(++step, "checking for concurrent readers/writers"); 

  static uint v;
  if (id == 0) { // CPU 0负责获取写锁，并在临界区内修改共享变量v，验证是否存在并发的读者和写者
    uint maxwv = 0; // 记录在写锁保护的临界区内，v的最大值，用于检测是否存在并发的读者和写者
    for (int i = 0; i < 1000000; i++) { // 大量调用write_acquire来增加触发竞争条件的概率
      write_acquire(&l); // 获取写锁
      // 将v的值加1，并获取加1后的值，验证是否存在并发的读者和写者
      uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
      // 如果x的值大于maxwv，说明在写锁保护的临界区内，v的值被其他CPU修改了，存在并发的读者和写者 
      if (x > maxwv) {
        maxwv = x;
      }
      // 获取v的当前值，并将v的值减1，验证是否存在并发的读者和写者
      uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL); 
      // 如果y的值大于maxwv，说明在写锁保护的临界区内，v的值被其他CPU修改了，存在并发的读者和写者
      if (y > maxwv) { 
        maxwv = y;
      }
      write_release(&l); // 释放写锁
    }
    // 如果maxwv的值大于1，说明在写锁保护的临界区内，v的值被其他CPU修改了多次，存在并发的读者和写者
    if (maxwv > 1) { 
      printf("rwspinlock_test: cpu %d saw concurrent reads/writes: %d\n", id, maxwv);
      r = -1;
    }
  } else { // 其他CPU负责获取读锁，并在临界区内修改共享变量v，验证是否存在并发的读者和写者
    uint maxrv = 0;
    for (int i = 0; i < 1000000; i++) {
      read_acquire(&l); // 获取读锁
      // 将v的值加1，并获取加1后的值，验证是否存在并发的读者和写者
      uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
      if (x > maxrv) {
        maxrv = x;
      }
      // 获取v的当前值，并将v的值减1，验证是否存在并发的读者和写者
      uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
      if (y > maxrv) {
        maxrv = y;
      }
      read_release(&l);
    }
    // 如果maxrv的值小于2，说明在读锁保护的临界区内，v的值没有被其他CPU修改过，说明没有并发的读者和写者
    if (maxrv < 2) {
      printf("rwspinlock_test: cpu %d never saw concurrent reads: %d\n", id, maxrv);
      r = -1;
    }
  }

  // 检查是否存在并发的写者，验证读写锁的正确性
  rwspinlock_test_step(++step, "checking for concurrent writers");

  uint maxwv = 0;
  for (int i = 0; i < 1000000; i++) {
    write_acquire(&l);
    uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
    if (x > maxwv) {
      maxwv = x;
    }
    uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
    if (y > maxwv) {
      maxwv = y;
    }
    write_release(&l);
  }
  if (maxwv > 1) {
    printf("rwspinlock_test: cpu %d saw concurrent writes: %d\n", id, maxwv);
    r = -1;
  }

  // 测试多个锁的情况，验证读写锁在多个锁的情况下是否能够正确地进行同步
  rwspinlock_test_step(++step, "acquiring multiple locks");

  struct rwspinlock l2; 
  initrwlock(&l2); // 初始化第二个读写锁
  write_acquire(&l2); // 获取第二个读写锁
  read_acquire(&l); // 获取第一个读写锁

  // 测试多个锁的情况，验证读写锁在多个锁的情况下是否能够正确地进行同步
  rwspinlock_test_step(++step, "releasing multiple locks");

  write_release(&l2); // 释放第二个读写锁
  read_release(&l); // 释放第一个读写锁

  for (int i = 0; i < 10; i++) { 
    // 测试多个写者优先的情况，验证等待的写者是否能够优先获取锁
    rwspinlock_test_step(++step, "prepare read_acquire for multiple writer priority test");

    static uint writer_count;
    if (id == 3) { // CPU 3负责获取读锁，为后续的多个写者优先测试做准备
      writer_count = 0; // 初始化writer_count，记录等待的写者数量
      read_acquire(&l); // 获取读锁
      read_acquire(&l); // 获取读锁
    }

    // 测试多个写者优先的情况，验证等待的写者是否能够优先获取锁
    rwspinlock_test_step(++step, "multiple writer priority test");

    if (id == 0 || id == 1) {
      write_acquire(&l); // 获取写锁l
      delay();
      write_release(&l); // 释放写锁l
    }

    if (id == 2) {
      delay();
      // 在写者获取锁之后，读者尝试获取读锁，验证是否能够在写者释放锁后获取到最新的flag值
      read_acquire(&l);
      if (writer_count == 0) {
        // 如果writer_count的值为0，说明读者在写者获取锁之前获取了锁，违反了写者优先的原则
        printf("rwspinlock_test: reader sneaked ahead of both waiting writers\n");
        r = -1;
      }
      delay();
      delay();
      delay();
      read_release(&l);
    }

    if (id == 3) {
      delay();
      read_release(&l); // 释放读锁l
      delay();
      read_release(&l); // 释放读锁l

      delay();
      delay();

      // By this point, either one writer executed and CPU 2 is holding read lock,
      // or both writers executed.  Should never sneak ahead of second writer.
      // 到这里，CPU 2应该已经获取了读锁，或者两个写者都已经执行了
      // 不应该是第二个写者之前获取到读锁了 
      read_acquire(&l);
      if (writer_count != 2) {
        // 如果writer_count的值不为2，说明读者在第二个写者获取锁之前获取了锁，违反了写者优先的原则
        printf("rwspinlock_test: reader sneaked ahead of second waiting writer\n");
        r = -1;
      }
      read_release(&l);
    }
  }

  // 测试完成，输出测试结果
  rwspinlock_test_step(++step, "done");

  printf("rwspinlock_test(%d): %d\n", id, r);
  pop_off(); // 重新开启中断

  return r;
}
#endif

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  int old = intr_get();

  // disable interrupts to prevent an involuntary context
  // switch while using mycpu().
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}

// Read a shared 32-bit value without holding a lock
// 读取一个共享的32位值而不持有锁，使用原子操作确保读取的值是最新的，避免竞争条件
int
atomic_read4(int *addr) {
  uint32 val;
  __atomic_load(addr, &val, __ATOMIC_SEQ_CST);
  return val;
}

#ifdef LAB_LOCK
/**
 * @brief 将锁的信息格式化成字符串，供snprintf调用
 * 
 * @param buf 存储格式化字符串的缓冲区
 * @param sz 缓冲区的大小
 * @param lk 要格式化的锁
 * @return int 实际写入缓冲区的字符数，不包括终止符
 */
int
snprint_lock(char *buf, int sz, struct spinlock *lk)
{
  int n = 0;
  if(lk->n > 0) { // 只有当acquire()被调用过至少一次时才格式化锁的信息
    // 将锁的信息格式化成字符串，包含锁的名称、test-and-set调用次数和acquire()调用次数
    // 写入buf缓冲区，最多写入sz个字符
    n = snprintf(buf, sz, "lock: %s: #test-and-set %d #acquire() %d\n",
                 lk->name, lk->nts, lk->n);
  }
  return n; // 返回实际写入缓冲区的字符数，不包括终止符
}

/**
 * @brief 统计锁的信息，并将统计结果格式化成字符串，供snprintf调用
 * 
 * @param buf 存储格式化字符串的缓冲区
 * @param sz 缓冲区的大小
 * @return int 实际写入缓冲区的字符数，不包括终止符
 */
int
statslock(char *buf, int sz) {
  int n;
  int tot = 0;

  acquire(&lock_locks); // 获取lock_locks锁，保护locks数组，确保在统计锁的信息时不会有其他CPU修改locks数组
  n = snprintf(buf, sz, "--- lock kmem stats\n"); // 将统计信息的标题写入buf缓冲区，最多写入sz个字符
  for(int i = 0; i < NLOCK; i++) { // 遍历locks数组，统计所有锁的信息
    if(locks[i] == 0) // 如果当前锁的指针为0，说明该位置没有锁，继续遍历下一个位置
      break;
    // 只有当锁的名称为"kmem"时才统计该锁的信息
    if(strncmp(locks[i]->name, "kmem", strlen("kmem")) == 0) {
      // 将该锁的test-and-set调用次数累加到tot中，统计所有"kmem"锁的test-and-set调用次数
      tot += locks[i]->nts; 
      // 将该锁的信息格式化成字符串，并写入buf缓冲区，最多写入sz-n个字符，n记录已经写入的字符数
      n += snprint_lock(buf +n, sz-n, locks[i]); 
    }
  }
  
  n += snprintf(buf+n, sz-n, "--- top 5 contended locks:\n");
  // 统计竞争最激烈的前5个锁的信息
  int last = 100000000;
  // stupid way to compute top 5 contended locks
  for(int t = 0; t < 5; t++) {
    int top = 0;
    for(int i = 0; i < NLOCK; i++) {
      if(locks[i] == 0)
        break;
      if(locks[i]->nts > locks[top]->nts && locks[i]->nts < last) {
        top = i;
      }
    }
    n += snprint_lock(buf+n, sz-n, locks[top]);
    last = locks[top]->nts;
  }
  n += snprintf(buf+n, sz-n, "tot= %d\n", tot);
  release(&lock_locks);  
  return n;
}
#endif
