#ifndef NETSTORE_2_SOCKETS_HH
#define NETSTORE_2_SOCKETS_HH

#include <bits/types.h>
#include <string>
#include <netinet/in.h>
#include <netdb.h>
#include "common.hh"
#include <sys/socket.h>

namespace netstore::sockets {

class udp {
 private:
  struct ip_mreq ip_mreq;
  int sock;

  void open();

  void close();

 public:
  udp(const std::string &addr, in_port_t port);

  udp(in_port_t port);

  udp();

  ~udp();

  void bind_local(in_port_t port);

  void set_multicast(const std::string &addr);

  void unset_multicast();

  void set_ttl(int32_t value);

  void set_broadcast();

  void set_timeout(__time_t sec, __suseconds_t usec);

  template<typename C>
  void send(C &msg, struct sockaddr_in &ra) {
    if (sendto(sock, &msg, msg.size(), 0, (struct sockaddr *) &ra,
               sizeof(ra)) != msg.size())
      throw exception("udp::send");
  }

  template<typename C>
  ssize_t recv(C &msg, struct sockaddr_in &ra) {
    ssize_t rcv_len = 0;
    socklen_t remote_len = sizeof(struct sockaddr_in);
    rcv_len = recvfrom(sock, &msg, sizeof(msg), 0, (struct sockaddr *) &ra,
                       &remote_len);
    if (rcv_len < 0)
      //throw exception("udp::recv");
      std::cout << "udp::recv" << std::endl;
    return rcv_len;
  }
};

class tcp {
 public:
  tcp();
  tcp(uint32_t sock);

  void open();

  void close();

  ~tcp();

  tcp accept();

  void bind(in_port_t port = 0);

  void listen(uint8_t queue_length = 5);

  void connect(const std::string& addr, in_port_t port);

  in_port_t port();

  void write(const std::string& msg);
  ssize_t read();

  char* buffer();
 private:
  int sock;
  static constexpr size_t buffer_size = 1024 * 512;
  char _buffer[buffer_size];
};

}

#endif //NETSTORE_2_SOCKETS_HH
