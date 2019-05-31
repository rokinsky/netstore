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

  void discover() {

    printf("Sending request...\n");
/*    char buffer[BSIZE];
    bzero(buffer, BSIZE);*/
    cmd::simple simple { cmd::hello, 10 };

    if (sendto(sock, &simple, simple.size(), 0, (struct sockaddr*) &remote_address, sizeof(remote_address)) != simple.size())
      throw std::runtime_error("write");

    socklen_t remote_len;
    ssize_t rcv_len = 0;
    while (rcv_len >= 0) {
      printf("Waiting for response...\n");
      cmd::complex complex;
      rcv_len = recvfrom(sock, &complex, sizeof(complex), 0, (struct sockaddr*) &remote_address, &remote_len);
      if (rcv_len >= 0 && cmd::eq(complex.cmd, cmd::good_day) && complex.cmd_seq() == simple.cmd_seq()) {
        std::cout << "Found " << inet_ntoa(remote_address.sin_addr) << " (" << complex.data << ") with free space " << complex.param() << std::endl;
        //std::cout << complex.to_string() << std::endl;
      }
    }
    printf("Didn't get any response. Break request.\n");
  }

  void search(const std::string& pattern) {
    printf("Sending request...\n");
    cmd::simple simple { cmd::list, 10, pattern.data() };
    if (sendto(sock, &simple, simple.size(), 0, (struct sockaddr*) &remote_address, sizeof(remote_address)) != simple.size())
      throw std::runtime_error("write");

    socklen_t remote_len;
    ssize_t rcv_len = 0;
    while (rcv_len >= 0) {
      printf("Waiting for response...\n");
      memset(&simple, 0, sizeof(simple));
      rcv_len = recvfrom(sock, &simple, sizeof(simple), 0, (struct sockaddr*) &remote_address, &remote_len);
      if (rcv_len >= 0) {
        if (cmd::eq(simple.cmd, cmd::my_list) && simple.cmd_seq() == simple.cmd_seq()) {
          std::string list(simple.data);
          size_t offset = 0;
          size_t N = list.length();
          while (offset < N) {
            size_t start = offset;
            offset = list.find('\n', start) + 1;
            size_t end = offset - 1;
            if (offset - 1 == std::string::npos) {
              end = offset = N;
            }
            std::string filename(simple.data + start, simple.data + end);
            std::cout << filename << " (" << inet_ntoa(remote_address.sin_addr) << ")" << std::endl;
          }
        }
      }
    }
    printf("Didn't get any response. Break request.\n");
  }

  std::string remote_dotted_address;
  in_port_t remote_port;
  int sock;
  struct sockaddr_in remote_address;

  inline bool is_discover(const std::string& s) {
    return s == "discover";
  }

  inline bool is_search(const std::string& s, std::string& result) {
    /* TODO fix .jpg */
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^search( |(\\s\\w+)*)"));

    if (is_match)
      result = s.substr(std::min<size_t>(strlen("search "), s.length()));

/*    std::regex pattern(R"(^search(\s\w+)*)");
    for (auto i = std::sregex_iterator(s.begin(), s.end(), pattern); i != std::sregex_iterator(); ++i) {
      std::cout << "match: " << i->str() << '\n';
    }*/
    std::cout << "match: " << result << std::endl;

    //if (std::regex_search(s, match, std::regex("^search(\\s\\w+)*")))
    //  std::cout << "match: " << match[1] << '\n';

    return is_match;
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
    std::string param;
    if (is_discover(line)) {
      std::cout << "!!discover" << std::endl;
      discover();
    } else if (is_search(line, param)) {
      std::cout << "!!searched word: " << param << std::endl;
      search(param);
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
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv, sizeof(struct timeval));
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
