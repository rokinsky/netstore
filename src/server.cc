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
#include "common.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fstream>

#include "sockets.hh"
#include "aux.hh"
#include "cmd.hh"

#define MAX_UDP 65507
#define BSIZE         1024
#define REPEAT_COUNT  30

namespace netstore {

class server {
 public:
  server(std::string ma, uint16_t cp, int64_t ms, std::string sf, uint8_t t)
   : mcast_addr(std::move(ma))
   , cmd_port(cp)
   , max_space(ms)
   , shrd_fldr(std::move(sf))
   , timeout(t)
   , udp(mcast_addr, cmd_port)
  {
    available_space = std::max<int64_t>(0, max_space - index_files());
    std::cout << "available space: " << available_space << std::endl;
  }

  void run();

  ~server() {
    udp.unset_multicast();
  }

 private:
  std::string mcast_addr;
  in_port_t cmd_port;
  int64_t max_space;
  std::string shrd_fldr;
  uint64_t available_space;
  uint8_t timeout;
  sockets::udp udp;

  std::unordered_map<std::string, uint64_t> files;

  void hello(sockaddr_in& ra, uint64_t cmd_seq);
  void list(sockaddr_in& ra, uint64_t cmd_seq, const std::string& s);
  void get(sockaddr_in& ra, uint64_t cmd_seq, const std::string& s);
  void del();
  void add();

  uint64_t index_files();

  ssize_t read_cmd(cmd::simple& cmd, sockaddr_in& remote);
};

uint64_t server::index_files() {
  namespace fs = std::filesystem;
  uint64_t total_size = 0;
  for (auto& p: fs::directory_iterator(shrd_fldr)) {
    if (p.is_regular_file()) {
      files[p.path().filename().string()] = p.file_size();
      total_size += p.file_size();
      std::cout << p.path().filename().string() << "(" << p.file_size() << ")" << '\n';
    }
  }
  std::cout << "total size: " << total_size << std::endl;
  return total_size;
}

ssize_t server::read_cmd(cmd::simple& cmd, sockaddr_in& remote) {
  ssize_t rcv_len = udp.recv(cmd, remote);
  if (rcv_len < 0)
    throw exception("read");
  return rcv_len;
}

void server::hello(sockaddr_in& ra, uint64_t cmd_seq) {
  cmd::complex complex(cmd::good_day, cmd_seq, available_space, mcast_addr.c_str());
  udp.send(complex, ra);
}

void server::list(sockaddr_in& ra, uint64_t cmd_seq, const std::string& s) {
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
    udp.send(simple, ra);
  }
}

void server::get(sockaddr_in& ra, uint64_t cmd_seq, const std::string& s) {
  if (s.empty() || s.find('/', 0) != std::string::npos)
    return;

  sockets::tcp tcp;
  tcp.bind();
  tcp.listen();
  cmd::complex complex(cmd::connect_me, cmd_seq, tcp.port(), s.data());
  udp.send(complex, ra);

  auto msg_tcp = tcp.accept();
  std::ifstream file;
  const auto path = shrd_fldr + "/" + s;
  file.open(path, std::ifstream::binary | std::ifstream::in);
  auto file_size = std::filesystem::file_size(path);

  if (file.is_open()) {
    while (file_size > 0) {
      auto nread = std::min<std::streamsize>(sockets::tcp::buffer_size, file_size);
      file.read(msg_tcp.buffer(), nread);
      msg_tcp.write(file.gcount());
      file_size -= file.gcount();
    }

    file.close();
    std::cout << "file send" << std::endl;
  } else {
    std::cout << "error!!! file send" << std::endl;
  }
}

void server::run() {
  for (auto i = 0; i < REPEAT_COUNT; ++i) {
    cmd::simple simple;
    sockaddr_in remote_address{};
    ssize_t rcv_len = read_cmd(simple, remote_address);

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
} // namespace netstore

int main(int ac, char** av) {
  namespace bpo = boost::program_options;
  std::string mcast_addr;
  int64_t cmd_port;
  int64_t max_space;
  std::string shrd_fldr;
  int64_t timeout;

  bpo::options_description desc("Allowed options");
  desc.add_options()
  (",g", bpo::value(&mcast_addr)->required(), "Multicast address")
  (",p", bpo::value(&cmd_port)->required()->notifier(
       boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, std::numeric_limits<uint16_t>::max(), "p")), "UDP port")
  (",b", bpo::value(&max_space)->default_value(52428800)->notifier(
       boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, std::numeric_limits<int64_t>::max(), "b")), "Allowed space")
  (",f", bpo::value(&shrd_fldr)->required()->notifier(boost::bind(&netstore::aux::check_dir, _1)),"Shared folder")
  (",t", bpo::value(&timeout)->default_value(5)->notifier(boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, 300, "t")), "Timeout")
  ;

  try {
    bpo::variables_map vm;
    store(bpo::parse_command_line(ac, av, desc), vm);
    notify(vm);

    netstore::server s(mcast_addr, cmd_port, max_space, shrd_fldr, timeout);
    std::cout << mcast_addr << " " << cmd_port << std::endl;
    s.run();
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    exit(EXIT_FAILURE);
  }
}
