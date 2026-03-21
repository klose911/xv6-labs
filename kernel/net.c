#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};

static struct spinlock netlock;

#define NPORTS 16 // 最大端口数量 
static struct port ports[NPORTS]; // 端口数组 

void netinit(void)
{
  initlock(&netlock, "netlock");
  for (int i = 0; i < NPORTS; i++) {
    ports[i].number = -1; // 初始化端口号为 -1，表示未绑定
  }
}
//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //
  int port;
  argint(0, &port); // parse port number

  if (port < 0 || port > 65535) 
    panic("sys_bind: invalid port number");

  acquire(&netlock); 
  for (int i = 0; i < NPORTS; i++) {
    if (ports[i].number == port) {
      release(&netlock);
      return -1;  // 端口已经被绑定了，返回错误
    }
  }

  for (int i = 0; i < NPORTS; i++) {
    if (ports[i].number == -1) { // 找到一个未绑定的端口
      ports[i].number = port; // 绑定端口号
      ports[i].head = 0; // 初始化 segment_queue 的头部索引，指向下一个要处理的数据包
      ports[i].tail = 0; // 初始化 segment_queue 的尾部索引
      memset(ports[i].segment_queue, 0, sizeof(ports[i].segment_queue)); // 初始化数据包队列
      release(&netlock);
      
      return 0; // 成功绑定端口，返回 0
    }
  }

  release(&netlock);
  return -1; // 没有可用的端口了，返回错误
}

/**
 * netlock 必须被持有才能调用这个函数
 */
static struct port *find_port(int port) {
  for (int i = 0; i < NPORTS; i++) {
    if (ports[i].number == port) {
      printf("find_port: found port %d at index %d\n", port, i); // 调试输出，显示找到的端口号和索引
      return &(ports[i]); // 找到对应端口，返回指向该端口的指针
    }
  }
  return 0; // 没有找到对应端口，返回 NULL
}
  //
  // unbind(int port)
  // release any resources previously created by bind(port);
  // from now on UDP packets addressed to port should be dropped.
  //
  uint64
  sys_unbind(void)
  {
    //
    // Optional: Your code here.
    //

    return 0;
  }

  //
  // recv(int dport, int *src, short *sport, char *buf, int maxlen)
  // if there's a received UDP packet already queued that was
  // addressed to dport, then return it.
  // otherwise wait for such a packet.
  //
  // sets *src to the IP source address.
  // sets *sport to the UDP source port.
  // copies up to maxlen bytes of UDP payload to buf.
  // returns the number of bytes copied,
  // and -1 if there was an error.
  //
  // dport, *src, and *sport are host byte order.
  // bind(dport) must previously have been called.
  //
  uint64
  sys_recv(void)
  {
    //
    // Your code here.
    //
    struct proc *proc = myproc(); 
    int dport;
    uint64 srcaddr;
    uint64 sportaddr;
    uint64 bufaddr;
    int maxlen;

    argint(0, &dport);    
    argaddr(1, &srcaddr);      
    argaddr(2, &sportaddr);    
    argaddr(3, &bufaddr); 
    argint(4, &maxlen);      
    
    acquire(&netlock); 

    struct port *port = find_port(dport); 
    if (!port) { 
      release(&netlock); 
      return -1; // 目的端口没有被绑定，返回错误
    } 

    while (port->head == port->tail) { 
      sleep(port, &netlock); // 等待直到有数据包到达
    } 

    if (port->head != port->tail) { 
      char *packet = port->segment_queue[port->head]; 
      port->head = (port->head + 1) % PORT_MAX_QUEUE; 

      struct eth *eth = (struct eth *) packet; 
      struct ip *ip = (struct ip *)(eth + 1); 
      struct udp *udp = (struct udp *)(ip + 1); 

      uint32 src_ip = ntohl(ip->ip_src); 
      uint16 src_port = ntohs(udp->sport); 
      int payload_len = ntohs(udp->ulen) - sizeof(struct udp); 
      if (payload_len > maxlen) {
        payload_len = maxlen; // 如果数据包的负载长度超过用户缓冲区的最大长度，则只复制 maxlen 字节
      }

      pagetable_t pagetable = proc->pagetable;
      if (copyout(pagetable, srcaddr, (char *)&src_ip, sizeof(src_ip)) < 0 ||
          copyout(pagetable, sportaddr, (char *)&src_port, sizeof(src_port)) < 0 ||
          copyout(pagetable, bufaddr, (char *)(udp + 1), payload_len) < 0) {
        kfree(packet); // 拷贝失败，丢弃数据包
        release(&netlock);
        return -1;
      }

      kfree(packet); // 处理完毕，释放数据包的内存
      release(&netlock);
      return payload_len; // 返回复制到用户空间的字节数
    }

    release(&netlock);
    return -1;
  }

  // This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
  // of the University of California.

  /**
   * @brief 计算校验码
   *
   * @param addr 数据缓冲区的地址，包含需要计算校验码的数据
   * @param len 数据缓冲区的长度，单位为字节
   * @return unsigned short 计算后的校验码
   */
  static unsigned short
  in_cksum(const unsigned char *addr, int len)
  {
    int nleft = len;
    const unsigned short *w = (const unsigned short *)addr;
    unsigned int sum = 0;
    unsigned short answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)
    {
      sum += *w++;
      nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
    {
      *(unsigned char *)(&answer) = *(const unsigned char *)w;
      sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum >> 16);
    /* guaranteed now that the lower 16 bits of sum are correct */

    answer = ~sum; /* truncate to 16 bits */
    return answer;
  }

  //
  // send(int sport, int dst, int dport, char *buf, int len)
  //

  /**
   * @brief 通过网络发送数据包的系统调用
   *
   * @param sport 本地端口
   * @param dst 目的服务器IP地址
   * @param dport 目的服务器端口
   * @param buf 数据缓存区指针
   * @param len 网络数据包长度
   *
   * @return uint64 发送结果，成功返回 0, 失败返回 -1
   *
   */
  uint64
  sys_send(void)
  {
    struct proc *p = myproc(); // 获取当前进程的指针
    int sport;
    int dst;
    int dport;
    uint64 bufaddr;
    int len;

    argint(0, &sport);    // 从系统调用参数中获取本地端口号
    argint(1, &dst);      // 从系统调用参数中获取目的服务器IP地址
    argint(2, &dport);    // 从系统调用参数中获取目的服务器端口
    argaddr(3, &bufaddr); // 从系统调用参数中获取数据缓存区指针
    argint(4, &len);      // 从系统调用参数中获取网络数据包长度

    // 计算数据包的总长度：
    // 数据长度 + 以太网头部长度 + IP头部长度 + UDP 头部长度
    int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
    if (total > PGSIZE) // 超过一页大小
      return -1;

    char *buf = kalloc(); // 分配一页新内存
    if (buf == 0)
    { // 分配失败
      printf("sys_send: kalloc failed\n");
      return -1;
    }
    memset(buf, 0, PGSIZE); // 清空新分配的内存

    // 设置以太网头部
    struct eth *eth = (struct eth *)buf;
    memmove(eth->dhost, host_mac, ETHADDR_LEN);
    memmove(eth->shost, local_mac, ETHADDR_LEN);
    eth->type = htons(ETHTYPE_IP);

    // 设置IP头部
    struct ip *ip = (struct ip *)(eth + 1);
    ip->ip_vhl = 0x45; // version 4, header length 4*5
    ip->ip_tos = 0;
    ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
    ip->ip_id = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 100;
    ip->ip_p = IPPROTO_UDP;
    ip->ip_src = htonl(local_ip);
    ip->ip_dst = htonl(dst);
    ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

    // 设置UDP头部
    struct udp *udp = (struct udp *)(ip + 1);
    udp->sport = htons(sport);
    udp->dport = htons(dport);
    udp->ulen = htons(len + sizeof(struct udp));

    char *payload = (char *)(udp + 1);
    // 从用户虚拟内存把数据拷贝到内核地址 payload
    if (copyin(p->pagetable, payload, bufaddr, len) < 0)
    {
      kfree(buf); // 拷贝失败，释放分配的buf
      printf("send: copyin failed\n");
      return -1;
    }

    e1000_transmit(buf, total); // 发送UDP包

    return 0; // 返回0 表示成功
  }

  void
  ip_rx(char *buf, int len)
  {
    // don't delete this printf; make grade depends on it.
    static int seen_ip = 0;
    if (seen_ip == 0)
      printf("ip_rx: received an IP packet\n");
    seen_ip = 1;

    //
    // Your code here.
    //
    acquire(&netlock); 

    struct eth *ineth = (struct eth *) buf;       // 收到的以太网帧头部
    struct ip *ip = (struct ip *)(ineth + 1);     // 收到的IP包头部 紧接着以太网头部
    if (ip->ip_p != IPPROTO_UDP) { // 只处理 UDP 包
      kfree(buf); // 不是 UDP 包，丢弃
      release(&netlock);
      return;
    } 
    
    struct udp *udp = (struct udp *)(ip + 1);    // 收到的UDP包头部 紧接着IP头部
    uint16 dport = ntohs(udp->dport); // 目的端口号
    struct port *port = find_port(dport); // 查找目的端口号对应的 inet_port 结构体
    // 目的端口没有被绑定，或者端口的 segment_queue 已经满了 
    if (port == 0 || (port->tail + 1) % PORT_MAX_QUEUE == port->head) { 
      kfree(buf); // 丢弃
      release(&netlock);
      return;
    }
    
    port->segment_queue[port->tail] = buf; // 把收到的数据包放入目的端口的 segment_queue 中
    port->tail = (port->tail + 1) % PORT_MAX_QUEUE; // 更新 segment_queue 的尾部索引
    wakeup(port); // 唤醒等待这个端口的进程 
    
    release(&netlock);
  }

  //
  // send an ARP reply packet to tell qemu to map
  // xv6's ip address to its ethernet address.
  // this is the bare minimum needed to persuade
  // qemu to send IP packets to xv6; the real ARP
  // protocol is more complex.
  // 发送一个ARP 请求包 告诉 qemu 将 xv6 的 IP 地址映射到它的以太网地址
  // 这是说服 qemu 将 IP 包发送到 xv6 的最低限度；真正的 ARP 协议更复杂
  void
  arp_rx(char *inbuf)
  {
    // 当第一次接收到 ARP 包时，seen_arp 被设置为 1，以后再接收到 ARP 包时就不会再次处理了
    // 这是因为在这个简单的实现中，我们只需要处理一次 ARP 包来告诉 qemu 映射 IP 地址到以太网地址，之后就不需要再处理了。
    static int seen_arp = 0; // 这个静态变量 seen_arp 用于跟踪是否已经处理过 ARP 包。初始值为 0，表示还没有处理过 ARP 包

    if (seen_arp)
    {               // 已经处理过arp了
      kfree(inbuf); // 释放数据缓存区
      return;
    }
    printf("arp_rx: received an ARP packet\n");
    seen_arp = 1; // 设置 seen_arp 为 1，表示已经处理过 ARP 包了

    struct eth *ineth = (struct eth *)inbuf;       // 收到的以太网帧头部
    struct arp *inarp = (struct arp *)(ineth + 1); // 收到的ARP 包头部 紧接着以太网头部

    char *buf = kalloc();      // 分配一个新的数据缓存区来构建 ARP 回复包
    if (buf == 0)              // 分配失败
      panic("send_arp_reply"); // 内核奔溃

    struct eth *eth = (struct eth *)buf; // 返回的以太网头部指向新分配的缓存区
    // 从“ARP请求的以太网帧头部” 复制 “源以太网地址” 到 “回复包的” “目的以太网地址”字段
    memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
    // 把“本机的以太网地址”复制到“回复包的” “源以太网地址”字段
    memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
    // 注意：这里需要使用 htons 转换成网络字节序
    eth->type = htons(ETHTYPE_ARP); // 设置以太网类型字段

    // 构建ARP回应的ARP 头部
    struct arp *arp = (struct arp *)(eth + 1);
    arp->hrd = htons(ARP_HRD_ETHER); // 硬件地址类型为以太网 0x1
    arp->pro = htons(ETHTYPE_IP);    // 协议类型为 IP 0x8000
    arp->hln = ETHADDR_LEN;          // 硬件地址长度为以太网地址长度 6字节
    arp->pln = sizeof(uint32);       // 协议地址长度为 IPv4 地址长度 4字节
    arp->op = htons(ARP_OP_REPLY);   // 操作类型为 ARP 回复 2

    // 拷贝“本机硬件地址”到 “发送者硬件地址”字段
    memmove(arp->sha, local_mac, ETHADDR_LEN);
    arp->sip = htonl(local_ip); // 设置发送者ip地址字段为 local_ip (10.0.2.5)
    // 拷贝请求者的硬件地址到 arp->tha字段上
    memmove(arp->tha, ineth->shost, ETHADDR_LEN);
    arp->tip = inarp->sip; // 设置请求者的IP字段

    e1000_transmit(buf, sizeof(*eth) + sizeof(*arp)); // 通过驱动发送ARP 响应包

    kfree(inbuf); // 清空 ARP 请求包的数据缓存区
  }

  void
  net_rx(char *buf, int len)
  {
    struct eth *eth = (struct eth *)buf; // 以太网帧头部

    // 根据以太网类型字段分发处理函数
    // 首先校验：收到的数据总长度至少要能容纳“以太网头 + ARP 头”
    // 接着检查以太网类型字段是否为 ARP
    // 这里用 ntohs 是因为网络字节序是大端，而主机字节序可能不同，必须先转换再比较
    if (len >= sizeof(struct eth) + sizeof(struct arp) &&
        ntohs(eth->type) == ETHTYPE_ARP)
    {
      arp_rx(buf); // 处理 ARP 包
    }
    else if (len >= sizeof(struct eth) + sizeof(struct ip) &&
             ntohs(eth->type) == ETHTYPE_IP)
    {
      ip_rx(buf, len); // 处理 IP 包
    }
    else
    {
      kfree(buf); // 不认识的包，丢弃
    }
  }
