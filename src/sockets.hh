#ifndef _SOCKETS_HH
#define _SOCKETS_HH

#include <bits/types.h>
#include <string>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <thread>

#include "common.hh"
#include "aux.hh"

namespace netstore::sockets {

static constexpr timeval default_to = {5, 0};

class udp {
 private:
  ::ip_mreq ip_mreq;
  int sock;

  void open();

  void close();

 public:
  /* only for multicast usage */
  udp(const std::string &mcast_addr, in_port_t port);

  explicit udp(in_port_t port);

  udp();

  ~udp();

  void bind_local(in_port_t port);

  void set_multicast(const std::string &addr);

  void unset_multicast();

  void set_ttl(int32_t value);

  void set_broadcast();

  void set_timeout(const timeval& tv = default_to);

  template<typename C>
  void send(C& msg, sockaddr_in& ra) {
    sendto(sock, &msg, msg.size(), 0, (sockaddr *) &ra, sizeof(ra));
  }

  template<typename C>
  ssize_t recv(C& msg, sockaddr_in& ra) {
    socklen_t remote_len = sizeof(sockaddr_in);
    return recvfrom(sock, &msg, sizeof(msg), 0, (sockaddr *) &ra,
                       &remote_len);
  }

  template <typename Func>
  void do_until(uint8_t timeout, Func&& func) {
    using namespace std::chrono;
    const auto end = system_clock::now()
                   + duration_cast<milliseconds>(seconds(timeout));
    auto ttl = [&end] () {
      return duration_cast<milliseconds>(end - system_clock::now());
    };

    for (auto left = ttl(); left.count() > 0; left = ttl()) {
      set_timeout(aux::to_timeval(left));
      func();
    }
  }
};

class tcp {
 public:
  static constexpr size_t bsize = 4096 * 512; /* 2 MiB */

  tcp();
  explicit tcp(uint32_t sock);

  void open();

  void close();

  ~tcp();

  tcp accept();

  void bind(in_port_t port = 0);

  void listen(uint8_t queue_length = 5);

  void connect(const std::string& addr, in_port_t port);

  in_port_t port();

  void write(ssize_t n = bsize);

  ssize_t read();

  char* buffer();

  void download(const std::string& path,
                const std::atomic<bool>& quit = netstore::quit);

  void upload(const std::string& path,
              const std::atomic<bool>& quit = netstore::quit);

  void set_timeout(const timeval& tv = default_to);

 private:
  int sock;
  char _buffer[bsize];
};

} // namespace netstore::sockets

#endif // _SOCKETS_HH
