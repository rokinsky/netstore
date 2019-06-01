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

namespace netstore::sockets {
  class udp {

  };
}

namespace netstore {

class server {
 public:
  server(std::string ma, uint16_t cp, int64_t ms, std::string sf, uint8_t t) :
      MCAST_ADDR(std::move(ma)),
      CMD_PORT(cp),
      MAX_SPACE(ms),
      SHRD_FLDR(std::move(sf)),
      TIMEOUT(t),
      sock(0),
      ip_mreq({}) {
    available_space = std::max<int64_t>(0, MAX_SPACE - index_files());
    std::cout << "available space: " << available_space << std::endl;
  }

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
  uint64_t available_space;
  uint8_t TIMEOUT;
  int sock;
  struct ip_mreq ip_mreq;

  std::unordered_map<std::string, uint64_t> files;

  void hello(struct sockaddr_in& ra, uint64_t cmd_seq);
  void list(struct sockaddr_in& ra, uint64_t cmd_seq, const std::string& s);
  void get(struct sockaddr_in& ra, uint64_t cmd_seq, const std::string& s);
  void del();
  void add();

  uint64_t index_files() {
    namespace fs = std::filesystem;
    uint64_t total_size = 0;
    for (auto& p: fs::directory_iterator(SHRD_FLDR)) {
      if (p.is_regular_file()) {
        files[p.path().filename().string()] = p.file_size();
        total_size += p.file_size();
        std::cout << p.path().filename().string() << "(" << p.file_size() << ")" << '\n';
      }
    }
    std::cout << "total size: " << total_size << std::endl;
    return total_size;
  }

  ssize_t read_cmd(cmd::simple& cmd, struct sockaddr_in& remote, socklen_t& remote_len) {
    remote_len = sizeof(struct sockaddr_in); /* WARNING !!! */
    ssize_t rcv_len = recvfrom(sock, &cmd, sizeof(cmd::simple), 0,
                       (struct sockaddr *) &remote, &remote_len);
    if (rcv_len < 0)
      throw exception("read");
    return rcv_len;
  }

  void open_socket() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
      throw exception("open_socket");
  }

  void join_multicast() {
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(MCAST_ADDR.c_str(), &ip_mreq.imr_multiaddr) == 0)
      throw exception("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
      throw exception("setsockopt");
  }

  void bind_local() {
    struct sockaddr_in local_address = {};
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(CMD_PORT);
    if (bind(sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
      throw exception("bind");
  }

  void close() {
    ::close(sock);
  }

  void leave_multicast() {
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
      throw exception("setsockopt");
  }
};

void server::connect() {
  open_socket();
  join_multicast();
  bind_local();
}

void server::hello(struct sockaddr_in& ra, uint64_t cmd_seq) {
  cmd::complex complex(cmd::good_day, cmd_seq, available_space, MCAST_ADDR.c_str());
  if (sendto(sock, (char *) &complex, complex.size(), 0,
             (struct sockaddr *) &ra, sizeof(ra)) == -1) {
    throw exception("sendto");
  }
}

void server::list(struct sockaddr_in& ra, uint64_t cmd_seq, const std::string& s) {
  std::string list;
  std::cout << "searching for " << s << std::endl;
  for (const auto &[k, v]: files) {
    if (s.empty() || k.find(s) != std::string::npos) {
      list += k + '\n';
    }
  }

  size_t offset = 0;
  while (offset < list.length()) {
    size_t start = offset;
    offset = list.rfind('\n', offset + cmd::max_simpl_data) + 1;

    cmd::simple simple(cmd::my_list, cmd_seq, list.c_str() + start, offset - start - 1);
    if (sendto(sock, (char *) &simple, simple.size(), 0,
               (struct sockaddr *) &ra, sizeof(ra)) == -1) {
      throw exception("sendto");
    }
  }
}

void server::get(struct sockaddr_in& ra, uint64_t cmd_seq, const std::string& s) {
  // open socket
  int sock_tcp = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
  if (sock_tcp < 0)
    throw exception("socket");



  cmd::complex complex(cmd::connect_me, cmd_seq, 11111, s.data());
  if (sendto(sock, (char *) &complex, complex.size(), 0,
             (struct sockaddr *) &ra, sizeof(ra)) == -1) {
    throw exception("sendto");
  }

  // listen on socket
}

void server::run() {
  for (auto i = 0; i < REPEAT_COUNT; ++i) {
    cmd::simple simple;
    struct sockaddr_in remote_address{};
    socklen_t remote_len;
    ssize_t rcv_len = read_cmd(simple, remote_address, remote_len);

    printf("address: %s %d\n", inet_ntoa(remote_address.sin_addr),
           ntohs(remote_address.sin_port));
    printf("read %zd bytes: %s, seq: %lu\n", rcv_len, simple.cmd,
           simple.cmd_seq());
    printf("read data %s\n", simple.data);

    if (cmd::eq(simple.cmd, cmd::hello) && simple.is_empty_data()) {
      hello(remote_address, simple.cmd_seq());
    } else if (cmd::eq(simple.cmd, cmd::list)) {
      list(remote_address, simple.cmd_seq(), std::string(simple.data));
    } else if (cmd::eq(simple.cmd, cmd::get)) {
      get(remote_address, simple.cmd_seq(), std::string(simple.data));
    } else if (cmd::eq(simple.cmd, cmd::del)) {

    } else if (cmd::eq(simple.cmd, cmd::add)) {
      cmd::complex complex(&simple);
    } else {
      std::cerr << "[PCKG ERROR] Skipping invalid package from "
      << inet_ntoa(remote_address.sin_addr) << ":"
      << ntohs(remote_address.sin_port) << "." << std::endl;
    }
  }
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
  (",g", bpo::value(&MCAST_ADDR)->required(), "Multicast address")
  (",p", bpo::value(&CMD_PORT)->required()->notifier(
       boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, std::numeric_limits<uint16_t>::max(), "p")), "UDP port")
  (",b", bpo::value(&MAX_SPACE)->default_value(52428800)->notifier(
       boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, std::numeric_limits<int64_t>::max(), "b")), "Allowed space")
  (",f", bpo::value(&SHRD_FLDR)->required()->notifier(boost::bind(&netstore::aux::check_dir, _1)),"Shared folder")
  (",t", bpo::value(&TIMEOUT)->default_value(5)->notifier(boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, 300, "t")), "Timeout")
  ;

  try {
    bpo::variables_map vm;
    store(bpo::parse_command_line(ac, av, desc), vm);
    notify(vm);

    netstore::server s(MCAST_ADDR, CMD_PORT, MAX_SPACE, SHRD_FLDR, TIMEOUT);
    s.connect();
    std::cout << MCAST_ADDR << " " << CMD_PORT << std::endl;
    s.run();
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    exit(EXIT_FAILURE);
  }
}
