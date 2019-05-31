#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/access.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <regex>

#include "helper.hh"

#define BSIZE         256
#define TTL_VALUE     4
#define REPEAT_COUNT  3
#define SLEEP_TIME    5

namespace netstore {
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

  void hello() {
    unsigned int remote_len;
    ssize_t rcv_len;
    printf("Sending request...\n");
    char buffer[BSIZE];
    bzero(buffer, BSIZE);
    cmd::simple simple { cmd::hello, htobe64(10) };

    if (sendto(sock, &simple, simple.size(), 0, (struct sockaddr*) &remote_address, sizeof(remote_address)) != simple.size())
      throw std::runtime_error("write");

    printf("Waiting for response...\n");

    cmd::complex complex;
    rcv_len = recvfrom(sock, &complex, sizeof(complex), 0, (struct sockaddr*) &remote_address, &remote_len);
    if (rcv_len < 0) {
      printf("Didn't get any response. Repeating request.\n");
    } else {
      //printf("Time: %.*s\n", (int)rcv_len, buffer);
      std::cout << complex.to_string() << std::endl;
    }
  }

  std::string remote_dotted_address;
  in_port_t remote_port;
  int sock;
  struct sockaddr_in remote_address;

  inline bool is_discover(const std::string& s) {
    return s == "discover";
  }

  inline bool is_search(const std::string& s) {
    return std::regex_match(s, std::regex("^search($| |(\\s\\w+))*"));
  }

  inline bool is_fetch(const std::string& s) {
    return std::regex_match(s, std::regex("^fetch(\\s\\w+)+"));
  }

  inline bool is_upload(const std::string& s) {
    return std::regex_match(s, std::regex("^upload(\\s\\w+)+"));
  }

  inline bool is_remove(const std::string& s) {
    return std::regex_match(s, std::regex("^remove(\\s\\w+)+"));
  }

  inline bool is_exit(const std::string& s) {
    return s == "exit";
  }
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

  std::string line;
  std::getline(std::cin, line);

  while(!is_exit(line)) {
    if (is_discover(line)) {
      std::cout << "!!discover" << std::endl;
    } else if (is_search(line)) {
      std::cout << "!!search" << std::endl;
    } else if (is_fetch(line)) {
      std::cout << "!!fetch" << std::endl;
    } else if (is_upload(line)) {
      std::cout << "!!upload" << std::endl;
    } else if (is_remove(line)) {
      std::cout << "!!remove" << std::endl;
    }
    std::cout << line << std::endl;
    std::getline(std::cin, line);
  }
  for (auto i = 0; i < REPEAT_COUNT; ++i) {
    hello();
  }
  printf("Closing.\n");
  close();
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
}

int main(int ac, char** av) {
  namespace bpo = boost::program_options;

  netstore::client c(av[1], (in_port_t)atoi(av[2]));
  c.connect();
  c.run();

/*
  // create and open a character archive for output
  std::ofstream ofs("filename");

  // create class instance
  const netstore::cmd::simple g("GET_TIME", 59, "DATA");

  // save data to archive
  {
    boost::archive::text_oarchive oa(std::cout);
    // write class instance to archive
    oa << g;
    // archive and stream closed when destructors are called
  }
  std::cout << "Asd";

  // ... some time later restore the class instance to its orginal state
  netstore::cmd::simple newg;
  {
    // create and open an archive for input
    std::ifstream ifs("filename");
    boost::archive::text_iarchive ia(ifs);
    // read class state from archive
    ia >> newg;
    // archive and stream closed when destructors are called
  }
*/


  bpo::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ;

  return 0;
}
