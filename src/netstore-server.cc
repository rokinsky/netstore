#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helper.hh"
#include <unistd.h>


#define MAX_UDP 65507
#define BSIZE         1024
#define REPEAT_COUNT  30

class server {
 public:
  server(std::string ma, uint16_t cp, int64_t ms, std::string sf, uint8_t t) :
    MCAST_ADDR(std::move(ma)),
    CMD_PORT(cp),
    MAX_SPACE(ms),
    SHRD_FLDR(std::move(sf)),
    TIMEOUT(t),
    sock(0),
    ip_mreq({})
  {}

  void connect();

  void run();

  ~server() {
    leave_multicast();
    close();
  }

 private:
  std::string MCAST_ADDR;
  in_port_t CMD_PORT;
  int64_t MAX_SPACE;
  std::string SHRD_FLDR;
  uint8_t TIMEOUT;
  int sock;
  struct ip_mreq ip_mreq;
  void open_socket() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
      throw std::runtime_error(std::string("open_socket") + " " + std::strerror(errno));
  }

  void join_multicast() {
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(MCAST_ADDR.c_str(), &ip_mreq.imr_multiaddr) == 0)
      throw std::runtime_error("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
      throw std::runtime_error(std::string("setsockopt") + " " + std::strerror(errno));
  }

  void bind_local() {
    struct sockaddr_in local_address = {};
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(CMD_PORT);
    if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
      throw std::runtime_error(std::string("bind") + " " + std::strerror(errno));
  }

  void close() {
    ::close(sock);
  }

  void leave_multicast() {
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
      throw std::runtime_error(std::string("setsockopt") + " " + std::strerror(errno));
  }
};

void server::connect() {
  open_socket();
  join_multicast();
  bind_local();
}

void server::run() {
  char buffer[BSIZE];
  ssize_t rcv_len;
  time_t time_buffer;
  struct sockaddr_in remote_address({});
  for (auto i = 0; i < REPEAT_COUNT; ++i) {
    bzero(buffer, BSIZE);
    std::cout << "sizeof simple " << sizeof(netstore::cmd::simple) << std::endl;
    netstore::cmd::simple simple{};
    socklen_t remote_len = sizeof(struct sockaddr_in); /* WARNING !!! */
    rcv_len = recvfrom(sock, &simple, sizeof(netstore::cmd::simple), 0, (struct sockaddr*) &remote_address, &remote_len);
    if (rcv_len < 0)
      throw std::runtime_error(std::string("read") + " " + std::strerror(errno));
    else {
      printf("address: %s %d\n", inet_ntoa(remote_address.sin_addr), ntohs(remote_address.sin_port));
      printf("read %zd bytes: %s, seq: %lu\n", rcv_len, simple.cmd, be64toh(simple.cmd_seq));
      printf("read data %s\n", simple.data);
      if (strncmp(simple.cmd, "GET_TIME", 10) == 0) {
        bzero(buffer, BSIZE);
        time(&time_buffer);
        strncpy(buffer, ctime(&time_buffer), BSIZE);
        if (sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr*) &remote_address, remote_len) == -1) {
          throw std::runtime_error(std::string("sendto") + " " + std::strerror(errno));
        } else {
          std::cout << "Sent time: " << buffer << std::endl;
        }
      } else {
        std::cout << "Received unexpected bytes." << std::endl;
      }
    }
  }
}


template<typename T>
void check_range(const T& value, const T& min, const T& max, const std::string& param) {
  namespace bpo = boost::program_options;
  if (value < min || value > max) {
    throw bpo::validation_error(
        bpo::validation_error::invalid_option_value, param, std::to_string(value)
    );
  }
}

int main(int ac, char** av) {
  namespace bpo = boost::program_options;
  std::string MCAST_ADDR;
  int64_t CMD_PORT;
  int64_t MAX_SPACE;
  std::string SHRD_FLDR;
  int64_t TIMEOUT;

  bpo::options_description desc("Allowed options");
  desc.add_options()
      (",g",
       bpo::value(&MCAST_ADDR)
         ->required(),
       "Multicast address")
      (",p",
       bpo::value(&CMD_PORT)
         ->required()
         ->notifier(
           boost::bind(&check_range<int64_t>, _1, 0, std::numeric_limits<uint16_t>::max(), "p")
         ),
       "UDP port")
      (",b",
       bpo::value(&MAX_SPACE)
         ->default_value(52428800)
         ->notifier(
           boost::bind(&check_range<int64_t>, _1, 0, std::numeric_limits<int64_t>::max(), "b")
         ),
       "Allowed space")
      (",f",
       bpo::value(&SHRD_FLDR)
         ->required()
         ->notifier(
          [] (const std::string& s) {
            if (not std::filesystem::is_directory(s))
              throw bpo::validation_error(
                  bpo::validation_error::invalid_option_value, "f", s
              );
         }),
       "Shared folder")
      (",t",
       bpo::value(&TIMEOUT)
         ->default_value(5)
         ->notifier(
           boost::bind(&check_range<int64_t>, _1, 0, 300, "t")
         ),
       "Timeout")
      ;

  try {
    bpo::variables_map vm;
    store(bpo::parse_command_line(ac, av, desc), vm);
    notify(vm);

    server s(MCAST_ADDR, CMD_PORT, MAX_SPACE, SHRD_FLDR, TIMEOUT);
    s.connect();
    std::cout << MCAST_ADDR << " " << CMD_PORT << std::endl;
    s.run();
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
  }
}
