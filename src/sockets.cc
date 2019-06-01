#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <string>
#include <iostream>

#include "sockets.hh"
#include "common.hh"

namespace netstore::sockets {
  udp::udp(const std::string& addr, in_port_t port) : udp(port) {
    set_multicast(addr);
  }

  udp::udp(in_port_t port) : udp() {
    bind_local(port);
  }

  udp::udp() {
    open();
  }

  udp::~udp() {
    close();
  }

  void udp::open() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
      throw exception("open_socket");
  }

  void udp::bind_local(in_port_t port) {
    struct sockaddr_in local_address = {};
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
      throw exception("bind");
  }

  void udp::set_ttl(int32_t value) {
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&value, sizeof value) < 0)
      throw exception("setsockopt multicast ttl");
  }

  void udp::set_multicast(const std::string& addr) {
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(addr.c_str(), &ip_mreq.imr_multiaddr) == 0)
      throw exception("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
      throw exception("setsockopt");
  }

  void udp::unset_multicast() {
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
      throw exception("setsockopt");
  }

  void udp::close() {
    ::close(sock);
  }

  void udp::set_broadcast() {
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
      throw std::runtime_error("setsockopt broadcast");
  }

  void udp::set_timeout(__time_t sec, __suseconds_t usec) {
    struct timeval tv {sec, usec};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
  }

}