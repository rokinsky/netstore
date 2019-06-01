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
      throw exception("udp::open_socket");
  }

  void udp::bind_local(in_port_t port) {
    struct sockaddr_in local_address = {};
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
      throw exception("udp::bind");
  }

  void udp::set_ttl(int32_t value) {
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&value, sizeof value) < 0)
      throw exception("udp::setsockopt multicast ttl");
  }

  void udp::set_multicast(const std::string& addr) {
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(addr.c_str(), &ip_mreq.imr_multiaddr) == 0)
      throw exception("udp::inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
      throw exception("udp::setsockopt set multicast");
  }

  void udp::unset_multicast() {
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
      throw exception("udp::setsockopt unset multicast");
  }

  void udp::close() {
    ::close(sock);
  }

  void udp::set_broadcast() {
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
      throw std::runtime_error("udp::setsockopt broadcast");
  }

  void udp::set_timeout(__time_t sec, __suseconds_t usec) {
    struct timeval tv {sec, usec};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
  }

  tcp::tcp() : sock(0), addr_result(nullptr) { open(); }

  tcp::tcp(uint32_t sock) : sock(sock), addr_result(nullptr) {}

  tcp::~tcp() {
    close();
    if (addr_result != nullptr) freeaddrinfo(addr_result);
  }

  tcp tcp::accept() {
    struct sockaddr_in client_address{};
    socklen_t client_address_len = sizeof(client_address);

    int msg_sock =
        ::accept(sock, (struct sockaddr*)&client_address, &client_address_len);
    if (msg_sock < 0) {
      throw exception("tcp::accept");
    }

    return tcp(msg_sock);
  }

  void tcp::bind(in_port_t port) {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr = {.s_addr = htonl(INADDR_ANY)};
    sa.sin_port = port;
    if (::bind(sock, (struct sockaddr *) &sa, sizeof(sa)) < 0)
      throw exception("tcp::bind");
  }

  void tcp::listen(uint8_t queue_length) {
    // switch to listening (passive open)
    if (::listen(sock, queue_length) < 0) throw exception("tcp::listen");
  }

  void tcp::connect(const std::string& addr, in_port_t port) {
    //convert(addr, port);
    struct sockaddr_in sa{};
    sa.sin_port = port;
    sa.sin_family = AF_INET;
    if (inet_aton(addr.c_str(), &sa.sin_addr) == 0)
      throw exception("udp::inet_aton");

    socklen_t addr_len = sizeof(struct sockaddr_in);

/*    if (::connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
      throw exception("tcp::connect");*/

    if (::connect(sock, (struct sockaddr *) &sa, addr_len) < 0)
      throw exception("tcp::connect");
  }

  in_port_t tcp::port() {
    struct sockaddr_in addr{};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if (getsockname(sock, (struct sockaddr*) &addr, &addrlen) < 0)
      throw exception("tcp::getsockname");

    return addr.sin_port;
  }

  void tcp::open() {
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) throw exception("tcp::open_socket");
  }

  void tcp::close() {
    if (::close(sock) < 0) throw exception("tcp::close");
  }

  void tcp::write(const std::string& msg) {
    if ((size_t)::write(sock, msg.c_str(), msg.length()) != msg.length())
      throw exception("tcp::write");
  }

  ssize_t tcp::read(std::string& msg) {
    char res[256];
    ssize_t nread = ::read(sock, &res, 256);
    if (nread < 0) throw exception("tcp::read");
    msg = std::string(res);
    return nread;
  }
}