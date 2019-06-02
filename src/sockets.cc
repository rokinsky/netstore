#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <string>
#include <iostream>

#include "sockets.hh"
#include "common.hh"
#include <boost/filesystem.hpp>
#include <fstream>

namespace netstore::sockets {
  udp::udp(const std::string& mcast_addr, in_port_t port) : udp(port) {
    set_multicast(mcast_addr);
  }

  udp::udp(in_port_t port) : udp() { bind_local(port); }

  udp::udp() : ip_mreq({}), sock(0) { open(); }

  udp::~udp() { close(); }

  void udp::open() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
      throw exception("udp::open_socket");
  }

  void udp::bind_local(in_port_t port) {
    sockaddr_in local_address = {};
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(port);
    if (bind(sock, (sockaddr *) &local_address, sizeof local_address) < 0)
      throw exception("udp::bind");
  }

  void udp::set_ttl(int32_t value) {
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &value,
                   sizeof value) < 0)
      msg::err("udp::setsockopt multicast ttl");
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

  void udp::close() { ::close(sock); }

  void udp::set_broadcast() {
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &optval, sizeof optval) < 0)
      throw std::runtime_error("udp::setsockopt broadcast");
  }

  void udp::set_timeout(const timeval& tv) {
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval));
  }

  tcp::tcp() : sock(0), _buffer() { open(); }

  tcp::tcp(uint32_t sock) : sock(sock), _buffer() {}

  tcp::~tcp() { close(); }

  tcp tcp::accept() {
    sockaddr_in client_address{};
    socklen_t client_address_len = sizeof(client_address);

    auto msg_sock = ::accept(sock, (sockaddr*)&client_address, &client_address_len);
    if (msg_sock < 0) {
      throw exception("tcp::accept");
    }

    return tcp(msg_sock);
  }

  void tcp::bind(in_port_t port) {
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr = {.s_addr = htonl(INADDR_ANY)};
    sa.sin_port = htons(port);
    if (::bind(sock, (sockaddr *) &sa, sizeof(sa)) < 0)
      throw exception("tcp::bind");
  }

  void tcp::listen(uint8_t queue_length) {
    if (::listen(sock, queue_length) < 0) throw exception("tcp::listen");
  }

  void tcp::connect(const std::string& addr, in_port_t port) {
    sockaddr_in sa{};
    sa.sin_port = htons(port);
    sa.sin_family = AF_INET;
    if (inet_aton(addr.c_str(), &sa.sin_addr) == 0)
      throw exception("tcp::inet_aton");

    if (::connect(sock, (sockaddr *) &sa, sizeof(sockaddr_in)) < 0)
      throw exception("tcp::connect");
  }

  in_port_t tcp::port() {
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(sockaddr_in);
    if (getsockname(sock, (sockaddr*) &addr, &addrlen) < 0)
      throw exception("tcp::getsockname");

    return ntohs(addr.sin_port);
  }

  void tcp::open() {
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) throw exception("tcp::open_socket");
  }

  void tcp::close() {
    if (::close(sock) < 0) throw exception("tcp::close");
  }

  void tcp::write(ssize_t n) {
    if (::write(sock, _buffer, n) != n) throw exception("tcp::write");
  }

  ssize_t tcp::read() {
    ssize_t nread = ::read(sock, _buffer, bsize);
    if (nread < 0) throw exception("tcp::read");
    return nread;
  }

  char* tcp::buffer() { return _buffer; }

  void tcp::download(const std::string& path, const std::atomic<bool>& quit) {
    std::ofstream file(path, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);

    if (file.is_open()) {
      ssize_t nread;
      while (!quit && (nread = read()) > 0) {
        file.write(buffer(), nread);
      }

      file.close();
    }
  }

  void tcp::upload(const std::string& path, const std::atomic<bool>& quit) {
    std::ifstream file(path, std::ifstream::binary | std::ifstream::in);
    auto fsize = boost::filesystem::file_size(path);

    if (file.is_open()) {
      while (!quit && fsize > 0) {
        auto nread = std::min<std::streamsize>(sockets::tcp::bsize, fsize);
        file.read(buffer(), nread);
        write(file.gcount());
        fsize -= file.gcount();
      }

      file.close();
    }
  }

  void tcp::set_timeout(const timeval& tv) {
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval)) < 0)
      throw exception("tcp::set receive timeout");
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(timeval)) < 0)
      throw exception("tcp::set send timeout");
  }
}