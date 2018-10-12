#include <pcap.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <vector>
#include <thread>
#include <mutex>

#define PROMISCUOUS 0
#define NONPROMISCUOUS 1

std::mutex records_mutex;
std::vector<uint64_t> records(65536, 0);
std::string local_ipaddr("10.12.11.4"); // <-- replace it with your own IP address.

void callback(u_char *useless, const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
  auto ep = reinterpret_cast<const struct ether_header *>(packet);
  auto ether_type = ntohs(ep->ether_type);

  if (ether_type != ETHERTYPE_IP) {
    return;
  }

  packet += sizeof(struct ether_header);
  auto ip_header = reinterpret_cast<const struct ip *>(packet);
  std::string src_addr(inet_ntoa(ip_header->ip_src));
  std::string dst_addr(inet_ntoa(ip_header->ip_dst));
  ssize_t payload_size = pkthdr->len - 16;

  if (ip_header->ip_p == IPPROTO_TCP) {
    packet += sizeof(struct ip);
    auto tcp_header = reinterpret_cast<const struct tcphdr *>(packet);
    uint16_t port = 0;

    if (src_addr == local_ipaddr) {
      port = ntohs(tcp_header->th_sport);
    } else if (dst_addr == local_ipaddr) {
      port = ntohs(tcp_header->th_dport);
    } else {
      // ICMP maybe? seems not need to care about it.
      return;
    }

    records_mutex.lock();
    records[port] += payload_size;
    records_mutex.unlock();
  } else if (ip_header->ip_p == IPPROTO_UDP) {
    // T.B.D
  }
}

void print_stat_entry() {
  for (;;) {
    sleep(60);
    system("clear");
    for (uint16_t port: {80, 443, 3389, /* etc */}) {
      if (records[port] != 0) {
        printf("%s:%d: %u byte(s)\n", local_ipaddr.c_str(), port, records[port]);
      }
    }
  }
}

int main(int argc, char **argv)
{
  char *dev;
  char *net;
  char *mask;
  char errbuf[PCAP_ERRBUF_SIZE];
  struct bpf_program fp;
  pcap_t *pcd; // packet caputre descriptor.
  bpf_u_int32 netp;
  bpf_u_int32 maskp;
  struct in_addr net_addr, mask_addr;

  dev = pcap_lookupdev(errbuf);
  if (dev == NULL)
  {
      printf("%s\n", errbuf);
      exit(1);
  }
  printf("DEV : %s\n", dev);

  // Get netmask
  if (pcap_lookupnet(dev, &netp, &maskp, errbuf) == -1) {
    fprintf(stderr, "%s\n", errbuf);
    return 1;
  }
  net_addr.s_addr = netp;
  net = inet_ntoa(net_addr);
  printf("NET : %s\n", net);
  mask_addr.s_addr = maskp;
  mask = inet_ntoa(mask_addr);
  printf("MASK : %s\n", mask);

  // Get packet capture descriptor.
  pcd = pcap_open_live(dev, BUFSIZ, NONPROMISCUOUS, 1000, errbuf);
  if (pcd == NULL) {
    fprintf(stderr, "%s\n", errbuf);
    return 1;
  }

  // Set compile option.
  if (pcap_compile(pcd, &fp, "tcp", 0, netp) == -1) {
    fprintf(stderr, "compile error\n");
    return 1; }

  // Set packet filter role by compile option.
  if (pcap_setfilter(pcd, &fp) == -1) {
    fprintf(stderr, "set filter error\n");
    return 1;
  }

  std::thread thread(print_stat_entry);

  // Capture packet. When packet captured, call callback function.
  pcap_loop(pcd, 0, callback, NULL);
  return 0;
}

