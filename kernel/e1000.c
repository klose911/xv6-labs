#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"

#define TX_RING_SIZE 16 // 传输描述符环大小
// 传输描述符环
// 使用16字节对齐以满足硬件要求
// 使用__attribute__((aligned(16)))确保对齐
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16))); 

#define RX_RING_SIZE 16 // 接收描述符环大小
// 接收描述符环
// 使用16字节对齐以满足硬件要求
// 使用__attribute__((aligned(16)))确保对齐
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));

// remember where the e1000's registers live.
// 记住e1000寄存器的位置
static volatile uint32 *regs;

struct spinlock e1000_lock; // e1000自旋锁

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// this code loosely follows the initialization directions
// in Chapter 14 of Intel's Software Developer's Manual.
// pci_init()调用。
// xregs是e1000寄存器映射的内存地址。
// 该代码大致遵循Intel软件开发手册第14章中的初始化说明
void e1000_init(uint32 *xregs) {
  int i;

  initlock(&e1000_lock, "e1000"); // 初始化e1000自旋锁

  regs = xregs; // 设置寄存器指针

  // Reset the device
  // 重置设备
  regs[E1000_IMS] = 0; // disable interrupts // 禁用中断
  regs[E1000_CTL] |= E1000_CTL_RST; // issue a reset // 发出重置命令
  regs[E1000_IMS] = 0; // redisable interrupts // 重新禁用中断
  __sync_synchronize(); // memory barrier

  // [E1000 14.5] Transmit initialization
  // 传输初始化
  memset(tx_ring, 0, sizeof(tx_ring)); // 清空传输描述符环
  for (i = 0; i < TX_RING_SIZE; i++) { // 初始化每个传输描述符
    tx_ring[i].status = E1000_TXD_STAT_DD; // mark descriptor done // 标记描述符已完成
    tx_ring[i].addr = 0; // buffer addr  // 缓冲区地址
  }
  regs[E1000_TDBAL] = (uint64)tx_ring; // transmit descriptor base address low // 传输描述符基址低
  if (sizeof(tx_ring) % 128 != 0) // must be multiple of 128 // 必须是128的倍数
    panic("e1000"); // 内核奔溃
  regs[E1000_TDLEN] = sizeof(tx_ring); // transmit descriptor length // 传输描述符长度
  regs[E1000_TDH] = regs[E1000_TDT] = 0; // head and tail // 头和尾置为NULL

  // [E1000 14.4] Receive initialization
  // 接收初始化
  memset(rx_ring, 0, sizeof(rx_ring)); // 清空接收描述符环
  for (i = 0; i < RX_RING_SIZE; i++) { // 初始化每个接收描述符
    rx_ring[i].addr = (uint64)kalloc(); // 为每个接收描述符分配缓冲区
    if (!rx_ring[i].addr) // 内存分配失败
      panic("e1000"); // 内核奔溃
  }
  regs[E1000_RDBAL] = (uint64)rx_ring; // receive descriptor base address low // 接收描述符基址低
  if (sizeof(rx_ring) % 128 != 0) // must be multiple of 128 // 必须是128的倍数
    panic("e1000"); // 内核奔溃
  regs[E1000_RDH] = 0; // receive descriptor head // 接收描述符头置为NULL
  regs[E1000_RDT] = RX_RING_SIZE - 1; // receive descriptor tail // 接收描述符尾置为最后一个描述符
  regs[E1000_RDLEN] = sizeof(rx_ring); // receive descriptor length // 接收描述符长度

  // filter by qemu's MAC address, 52:54:00:12:34:56
  // 按照qemu的MAC地址过滤，52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452; // 设置低32位MAC地址
  regs[E1000_RA + 1] = 0x5634 | (1 << 31); // 设置高16位MAC地址并启用该地址
  // multicast table 
  // 多播表
  for (int i = 0; i < 4096 / 32; i++) // 清空多播表
    regs[E1000_MTA + i] = 0; // clear multicast table

  // transmitter control bits.
  // 传输控制位
  regs[E1000_TCTL] = E1000_TCTL_EN |                 // enable // 启用传输
                     E1000_TCTL_PSP |                // pad short packets // 填充短包
                     (0x10 << E1000_TCTL_CT_SHIFT) | // collision stuff // 碰撞阈值
                     (0x40 << E1000_TCTL_COLD_SHIFT); // collision distance // 碰撞距离
  regs[E1000_TIPG] = 10 | (8 << 10) | (6 << 20); // inter-pkt gap // 包间隙

  // receiver control bits.
  // 接收控制位
  regs[E1000_RCTL] = E1000_RCTL_EN |      // enable receiver // 启用接收器
                     E1000_RCTL_BAM |     // enable broadcast // 启用广播
                     E1000_RCTL_SZ_2048 | // 2048-byte rx buffers // 2048字节接收缓冲区
                     E1000_RCTL_SECRC;    // strip CRC // 去除CRC

  // ask e1000 for receive interrupts.
  // 请求e1000接收中断
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer) // 每收到一个包就中断（无定时器）
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer) // 每收到一个包就中断（无定时器）
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back // 接收描述符写回
}

int
e1000_transmit(char *buf, int len)
{
  //
  // Your code here.
  //
  // buf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after send completes.
  //
  // return 0 on success.
  // return -1 on failure (e.g., there is no descriptor available)
  // so that the caller knows to free buf.
  //

  printf("transimited data packet to e1000 card: %s, %d\n", buf, len);

  acquire(&e1000_lock);
  uint32 tail = regs[E1000_TDT];

  if ((tx_ring[tail].status & E1000_TXD_STAT_DD) == 0) {
    release(&e1000_lock);
    return -1; 
  }

  if (!(tx_ring[tail].addr)) {
    kfree((void *) tx_ring[tail].addr);
  }

  // init the descriptor
  tx_ring[tail].addr = (uint64) buf;
  tx_ring[tail].length = len;
  tx_ring[tail].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;

   __sync_synchronize(); 

  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;
  __sync_synchronize(); 
  
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver a buf for each packet (using net_rx()).
  //
  printf("received data packet from e1000 card\n");

  acquire(&e1000_lock);

  while (1) {
    
    uint32 idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    if ((rx_ring[idx].status & E1000_TXD_STAT_DD) == 0) {
      release(&e1000_lock); 
      return;
    }

    net_rx((char *)rx_ring[idx].addr, rx_ring[idx].length);
    rx_ring[idx].addr = (uint64)kalloc();
    if (!rx_ring[idx].addr) {
      release(&e1000_lock);
      panic("e1000");
    }
    rx_ring[idx].length = 0;
    rx_ring[idx].status = 0;

    __sync_synchronize();

    regs[E1000_RDT] = idx;

    __sync_synchronize();
  }

  release(&e1000_lock);
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
