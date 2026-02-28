//
// E1000 hardware definitions: registers and DMA ring format.
// from the Intel 82540EP/EM &c manual.
//
#include "types.h"

#include "types.h"

/* Registers */
// 寄存器 
#define E1000_CTL      (0x00000/4)  /* Device Control Register - RW */ // 设备控制寄存器
#define E1000_ICR      (0x000C0/4)  /* Interrupt Cause Read - R */ // 中断原因 只读
#define E1000_IMS      (0x000D0/4)  /* Interrupt Mask Set - RW */ // 中断掩码设置 
#define E1000_RCTL     (0x00100/4)  /* RX Control - RW */ // 接收控制寄存器 
#define E1000_TCTL     (0x00400/4)  /* TX Control - RW */ // 传输控制寄存器 
#define E1000_TIPG     (0x00410/4)  /* TX Inter-packet gap -RW */ // 传输包间隙 
#define E1000_RDBAL    (0x02800/4)  /* RX Descriptor Base Address Low - RW */ // 接收描述符基地址低 
#define E1000_RDTR     (0x02820/4)  /* RX Delay Timer */  // 接收延迟定时器 
#define E1000_RADV     (0x0282C/4)  /* RX Interrupt Absolute Delay Timer */ // 接收中断绝对延迟定时器 
#define E1000_RDH      (0x02810/4)  /* RX Descriptor Head - RW */ // 接收描述符头
#define E1000_RDT      (0x02818/4)  /* RX Descriptor Tail - RW */ // 接收描述符尾
#define E1000_RDLEN    (0x02808/4)  /* RX Descriptor Length - RW */ // 接收描述符长度
#define E1000_TDBAL    (0x03800/4)  /* TX Descriptor Base Address Low - RW */ // 传输描述符基地址低
#define E1000_TDLEN    (0x03808/4)  /* TX Descriptor Length - RW */ // 传输描述符长度
#define E1000_TDH      (0x03810/4)  /* TX Descriptor Head - RW */ // 传输描述符头
#define E1000_TDT      (0x03818/4)  /* TX Descriptor Tail - RW */ // 传输描述符尾
#define E1000_MTA      (0x05200/4)  /* Multicast Table Array - RW Array */ // 多播表数组
#define E1000_RA       (0x05400/4)  /* Receive Address - RW Array */ // 接收地址数组

/* Device Control */
// 设备控制
#define E1000_CTL_RST     0x04000000    /* full reset */ // 完全重置

/* Transmit Control */
// 传输控制
#define E1000_TCTL_EN     0x00000002    /* enable tx */ // 启用传输
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */ // 填充短包
#define E1000_TCTL_CT_SHIFT 4 // collision threshold // 碰撞阈值
#define E1000_TCTL_COLD_SHIFT 12 // collision distance // 碰撞距离

/* Receive Control */
// 接收控制
#define E1000_RCTL_EN             0x00000002    /* enable */ // 启用
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */ // 广播启用
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */ // 接收缓冲区大小2048
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */ // 去除以太网CRC

/* Transmit Descriptor command definitions [E1000 3.3.3.1] */
// 传输描述符命令定义
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */ // 包结束
#define E1000_TXD_CMD_RS     0x08 /* Report Status */ // 报告状态

/* Transmit Descriptor status definitions [E1000 3.3.3.2] */
// 传输描述符状态定义
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */ // 描述符完成

// [E1000 3.3.3] Legacy Transmit Descriptor Format
// 传统传输描述符格式

/**
 * @brief 传统传输描述符结构体 
 * 
 */
struct tx_desc
{
  uint64 addr; // 指向数据缓冲区的地址
  uint16 length; // 数据缓冲区长度
  uint8 cso; // Checksum offset
  uint8 cmd; // 命令
  uint8 status; // 状态
  uint8 css; // Checksum start
  uint16 special; // Special field
};

/* Receive Descriptor bit definitions [E1000 3.2.3.1] */
// 接收描述符位定义
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */ // 描述符完成
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */ // 包结束

// [E1000 3.2.3]

/**
 * @brief 传统接收描述符结构体
 * 
 */
struct rx_desc
{
  uint64 addr;       /* Address of the descriptor's data buffer */ // 指向描述符数据缓冲区的地址
  uint16 length;     /* Length of data DMAed into data buffer */ // DMA传输到数据缓冲区的数据长度
  uint16 csum;       /* Packet checksum */ // 包校验和
  uint8 status;      /* Descriptor status */ // 描述符状态
  uint8 errors;      /* Descriptor Errors */ // 描述符错误
  uint16 special; // Special field
};

