#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#define BSIZE         256
#define TTL_VALUE     4
#define REPEAT_COUNT  3
#define SLEEP_TIME    5

class client {
 public:
   client(std::string address, in_port_t port)
   : remote_dotted_address(std::move(address))
   , remote_port(port)
   , sock(0)
   , remote_address({})
   {}

   ~client() {
     close();
   }

   void connect();
   void run();

 private:
  void open_socket();
  void set_broadcast();
  void set_ttl();
  void bind_local();
  void set_target();
  void set_timeout();
  void close();
  std::string remote_dotted_address;
  in_port_t remote_port;
  int sock;
  struct sockaddr_in remote_address;
};

void client::connect() {
  open_socket();
  set_broadcast();
  set_ttl();
  bind_local();
  set_target();
  set_timeout();
}

void client::run() {
  char buffer[BSIZE];
  size_t length;
  unsigned int remote_len;

  ssize_t rcv_len;
  for (auto i = 0; i < REPEAT_COUNT; ++i) {
    printf("Sending request...\n");
    bzero(buffer, BSIZE);
    strncpy(buffer, "GET_TIME", BSIZE);
    length = strnlen(buffer, BSIZE);
    if (sendto(sock, buffer, length, 0, (struct sockaddr*) &remote_address, sizeof(remote_address)) != length)
      throw std::runtime_error("write");

    printf("Waiting for response...\n");
    rcv_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*) &remote_address, &remote_len);
    if (rcv_len < 0) {
      if (i == 2) {
        printf("Closing.\n");
        close();
        exit(1);
      }
      printf("Didn't get any response. Repeating request.\n");
    } else {
      printf("Time: %.*s\n", (int)rcv_len, buffer);
      break;
    }
  }
}

void client::open_socket() {
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    throw std::runtime_error("socket");
}

void client::set_broadcast() {
  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
    throw std::runtime_error("setsockopt broadcast");
}

void client::set_ttl() {
  int optval = TTL_VALUE;
  if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
    throw std::runtime_error("setsockopt multicast ttl");
}

void client::bind_local() {
  struct sockaddr_in local_address({});
  local_address.sin_family = AF_INET;
  local_address.sin_addr.s_addr = htonl(INADDR_ANY);
  local_address.sin_port = htons(0);
  if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
    throw std::runtime_error("bind");
}

void client::set_target() {
  remote_address.sin_family = AF_INET;
  remote_address.sin_port = htons(remote_port);
  if (inet_aton(remote_dotted_address.c_str(), &remote_address.sin_addr) == 0)
    throw std::runtime_error("inet_aton");
}

void client::set_timeout() {
  struct timeval tv({});
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
}

void client::close() {
  ::close(sock);
}

int main(int ac, char** av) {
  namespace bpo = boost::program_options;

  client c(av[1], (in_port_t)atoi(av[2]));
  c.connect();
  c.run();

  bpo::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ;

  return 0;
}
