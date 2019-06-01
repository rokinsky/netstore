#ifndef NETSTORE_2_SOCKETS_HH
#define NETSTORE_2_SOCKETS_HH

#include <bits/types.h>
#include <string>
#include <netinet/in.h>
#include "helper.hh"

namespace netstore::sockets {

/*template <typename C>*/
class udp {
 public:
  int sock;

 private:
  struct ip_mreq ip_mreq;

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

/*  void send(C& msg, struct sockaddr_in& ra);
ssize_t recv(C& msg, struct sockaddr_in& ra);*/

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
}

#endif //NETSTORE_2_SOCKETS_HH
