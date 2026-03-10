//
// endianness support
//
// 字节序支持
#include "types.h"

/**
 * 这个内联函数 bswaps 的作用是对一个 16 位无符号整数做字节交换（byte swap）
 * 也就是把高 8 位和低 8 位互换，例如 0x1234 变成 0x3412
 * 这通常用于处理不同字节序（大端/小端）之间的数据转换，常见于网络协议和二进制文件解析。
 *  
 * @parma val 需要进行字节交换的 16 位无符号整数
 * 
 * @return 交换字节后的 16 位无符号整数 
 * 
 */
static inline uint16 bswaps(uint16 val)
{
  // 1. 先用 val & 0x00ffU 取出低字节并左移 8 位
  // 2. 再用 val & 0xff00U 取出高字节并右移 8 位
  // 3. 最后用按位或 | 合并成新值
  return (((val & 0x00ffU) << 8) |
          ((val & 0xff00U) >> 8));
}

/**
 * @brief 这个内联函数 bswapl 的作用是对一个 32 位无符号整数做字节交换（byte swap）
 * 也就是把高位字节和低位字节互换，例如 0x12345678 变成 0x78563412
 * 这通常用于处理不同字节序（大端/小端）之间的数据转换，常见于网络协议和二进制文件解析。
 *  
 * @param val 需要进行字节交换的 32 位无符号整数
 * 
 * @return 交换字节后的 32 位无符号整数
 * 
 */
static inline uint32 bswapl(uint32 val)
{
  // 1. 先用 val & 0x000000ffUL 取出最低字节并左移 24 位
  // 2. 再用 val & 0x0000ff00UL 取出次低字节并左移 8 位
  // 3. 再用 val & 0x00ff0000UL 取出次高字节并右移 8 位
  // 4. 最后用 val & 0xff000000UL 取出最高字节并右移 24 位
  // 5. 最后用按位或 | 合并成新值
  return (((val & 0x000000ffUL) << 24) |
          ((val & 0x0000ff00UL) << 8) |
          ((val & 0x00ff0000UL) >> 8) |
          ((val & 0xff000000UL) >> 24));
}

// Use these macros to convert network bytes to the native byte order.
// Note that Risc-V uses little endian while network order is big endian.
// 使用宏将网络字节序转换为本地字节序
// 注意： RISC-V 使用 little endian, 而网络字节序是 big endian
#define ntohs bswaps // network to host short
#define ntohl bswapl // network to host long
#define htons bswaps // host to network short
#define htonl bswapl // host to network long


//
// useful networking headers
// 
#define ETHADDR_LEN 6 // 以太网地址长度，6字节

// an Ethernet packet header (start of the packet).
// 以太网包头（数据包的开始）
// 告诉编译器不要对结构体进行内存对齐，以确保结构体的布局与网络协议规范完全一致
// 避免因填充字节导致的数据解析错误。  
struct eth {
  uint8  dhost[ETHADDR_LEN]; // 目的以太网地址
  uint8  shost[ETHADDR_LEN]; // 源以太网地址
  uint16 type; // 以太网类型字段，指示上层协议类型，例如 IP 或 ARP
} __attribute__((packed)); 

// IP 协议
#define ETHTYPE_IP  0x0800 // Internet protocol 
// ARP 协议
#define ETHTYPE_ARP 0x0806 // Address resolution protocol

// an IP packet header (comes after an Ethernet header)
// IP包的头部 (紧接着以太网头部)
struct ip {
  uint8  ip_vhl; // ip 版本 version << 4 | header length >> 2
  uint8  ip_tos; // 服务类型 type of service
  uint16 ip_len; // 数据包长度 total length, including this IP header
  uint16 ip_id;  // 标志位 identification
  // ip_off 的高 3 位是标志位 flags，低 13 位是片偏移 fragment offset
  uint16 ip_off; // fragment offset field 
  uint8  ip_ttl; // 生存时间 time to live
  uint8  ip_p;   // 协议 protocol
  uint16 ip_sum; // 校验和 checksum, covers just IP header
  uint32 ip_src, ip_dst; // 源 IP 地址和目的 IP 地址 
};

// 传输层协议
#define IPPROTO_ICMP 1  // ICMP 协议 (ping) Control message protocol
#define IPPROTO_TCP  6  // TCP 协议 Transmission control protocol
#define IPPROTO_UDP  17 // UDP 协议 User datagram protocol

/**
 * @brief  这个宏 MAKE_IP_ADDR 的作用创建一个ip地址
 * 
 * 将四个 8 位无符号整数（a、b、c、d）组合成一个 32 位无符号整数，表示一个 IPv4 地址。
 * 
 */
#define MAKE_IP_ADDR(a, b, c, d)           \
  (((uint32)a << 24) | ((uint32)b << 16) | \
   ((uint32)c << 8) | (uint32)d)

// a UDP packet header (comes after an IP header).
// UDP 头（紧挨着IP段后面）
struct udp {
  uint16 sport; // 源端口 source port
  uint16 dport; // 目的端口 destination port
  // 包含 udp 头部，但不包含 ip 段
  uint16 ulen;  // 数据长度 length, including udp header, not including IP header
  uint16 sum;   // 校验位 checksum 
};

// an ARP packet (comes after an Ethernet header).
// ARP 包（紧接着以太网头部）
struct arp {
  // hrd 和 pro 字段分别表示硬件地址类型和协议地址类型
  // 通常以太网和 IPv4 分别对应 1 和 0x0800
  uint16 hrd; // format of hardware address 
  uint16 pro; // format of protocol address
  uint8  hln; // 硬件地址长度 length of hardware address 
  uint8  pln; // 协议地址长度 length of protocol address
  // arp 操作：
  // 1. request ARP 请求 
  // 2. reply ARP 返回
  uint16 op;  // operation

  // 发送者的硬件地址 和 IP 地址
  char   sha[ETHADDR_LEN]; // sender hardware address
  uint32 sip;              // sender IP address
  // 目标的硬件地址和IP地址
  char   tha[ETHADDR_LEN]; // target hardware address
  uint32 tip;              // target IP address
} __attribute__((packed));

#define ARP_HRD_ETHER 1 // Ethernet

/**
 * ARP 操作类型
 */
enum {
  ARP_OP_REQUEST = 1, // requests hw addr given protocol addr
  ARP_OP_REPLY = 2,   // replies with the hw addr of the protocol addr
};

// an DNS packet (comes after an UDP header).
// DNS 包 （紧挨着 UDP 包）
struct dns {
  uint16 id;  // request ID 请求ID

  uint8 rd: 1;  // recursion desired 是否递归查询
  uint8 tc: 1;  // truncated 截断
  uint8 aa: 1;  // authoritive 

  // opcode: 
  // 0 for standard query
  // 1 for inverse query
  // 2 for server status request, etc.
  uint8 opcode: 4; // purpose of message 查询类型 
  uint8 qr: 1;  // query/response 0 查询 1 响应 
  // rcode: 
  // 0 for no error
  // 1 for format error
  // 2 for server failure
  // 3 for name error
  // 4 for not implemented
  // 5 for refused, etc.
  uint8 rcode: 4; // response code 响应代码
  uint8 cd: 1;  // checking disabled 是否校验
  uint8 ad: 1;  // authenticated data 是否权威
  uint8 z:  1;  // 保留
  uint8 ra: 1;  // recursion available 是否支持递归查询
  
  uint16 qdcount; // number of question entries
  uint16 ancount; // number of resource records in answer section
  uint16 nscount; // number of NS resource records in authority section
  uint16 arcount; // number of resource records in additional records
} __attribute__((packed));

/**
 * @brief DNS 查询结构体
 * 
 */
struct dns_question {
  // 1.  A 
  // 5.  CNAME  
  // 28  AAAA 
  // 33  SRV 
  // 255 ANY
  uint16 qtype; // query type 查询类型
  // 1 = IN (Internet) 
  uint16 qclass; // query class 查询类
} __attribute__((packed));
  
#define ARECORD (0x0001)
#define QCLASS  (0x0001)

/**
 * @brief dns 数据结构体
 * 
 */
struct dns_data {
  uint16 type; // 数据类型，例如 A 记录、CNAME 记录等
  uint16 class; // 数据类，通常为 IN (Internet)
  uint32 ttl; // 数据的生存时间，单位为秒，表示数据在 DNS 服务器中缓存的时间长度
  uint16 len; // 数据长度，表示数据字段的字节数
} __attribute__((packed));

struct inet_port {
  int port;
  char *segment_queue[16];
} __attribute__((packed));
