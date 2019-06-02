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
#include <atomic>
#include <mutex>

#include "sockets.hh"
#include "aux.hh"
#include "cmd.hh"

#define REPEAT_COUNT  30 /* TODO remove it! */

namespace netstore {

class server {
 public:
  server(std::string ma, uint16_t cp, uint64_t ms, std::string sf, uint8_t t)
   : mcast_addr(std::move(ma))
   , cmd_port(cp)
   , max_space(ms)
   , shrd_fldr(std::move(sf))
   , available_space(0)
   , timeout(t)
   , udp(mcast_addr, cmd_port)
  { set_space(index_files()); }

  void run();

  ~server() {
    udp.unset_multicast();
  }

 private:
  std::string mcast_addr;
  in_port_t cmd_port;
  uint64_t max_space;
  std::string shrd_fldr;

  std::mutex m; /* mutex for available space and files */
  uint64_t available_space;
  std::unordered_map<std::string, uint64_t> files;

  uint8_t timeout;
  sockets::udp udp;

  void hello(sockaddr_in& ra, uint64_t cmd_seq);
  void list(sockaddr_in& ra, uint64_t cmd_seq, const std::string& f);
  void get(sockaddr_in& ra, uint64_t cmd_seq, const std::string& f);
  void del(const std::string& f);
  void add(sockaddr_in& ra, uint64_t cmd_seq, uint64_t fsize, const std::string& f);

  uint64_t index_files();

  ssize_t read_cmd(cmd::simple& cmd, sockaddr_in& remote);

  void set_space(uint64_t value) {
    if (available_space > 0) throw std::logic_error("available space is not zero");
    available_space = max_space < value ? 0 : max_space - value;
  }

  void inc_space(uint64_t value) {
    available_space = max_space - available_space < value ? max_space : value + available_space;
  }

  void dec_space(uint64_t value) {
    available_space = max_space - available_space < value ? 0 : available_space - value;
  }
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
  std::unique_lock lk(m);
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

void server::get(sockaddr_in& ra, uint64_t cmd_seq, const std::string& f) {
  const auto path = aux::path(shrd_fldr, f);
  if (!aux::validate(f) || !aux::exists(path)) return;

  sockets::tcp tcp;
  tcp.bind();
  tcp.listen();
  tcp.set_timeout({timeout, 0});
  cmd::complex complex(cmd::connect_me, cmd_seq, tcp.port(), f.data());
  udp.send(complex, ra);

  auto msg_tcp = tcp.accept();
  msg_tcp.upload(path);
  msg::uploaded(f, inet_ntoa(ra.sin_addr), ra.sin_port);
}

void server::del(const std::string& f) {
  const auto path = aux::path(shrd_fldr, f);
  if (!aux::validate(f) || !aux::exists(path)) return;
  std::filesystem::remove(path);

  {
    std::unique_lock lk(m);
    inc_space(files[f]);
    files.erase(f);
  }
}

void server::add(sockaddr_in& ra, uint64_t cmd_seq, uint64_t fsize, const std::string& f) {
  const auto path = aux::path(shrd_fldr, f);
  {
    std::cout << "adding data: " << f << std::endl;
    std::unique_lock lk(m);
    if (!aux::validate(f) || files.find(f) != files.end() || fsize > available_space) {
      cmd::simple simple(cmd::no_way, cmd_seq, f.data());
      udp.send(simple, ra);
      return;
    }
    dec_space(fsize);
    files[f] = fsize;
  }
  sockets::tcp tcp;
  tcp.bind();
  tcp.listen();
  tcp.set_timeout({timeout, 0});
  cmd::complex complex(cmd::can_add, cmd_seq, tcp.port());
  udp.send(complex, ra);

  try {
    auto msg_tcp = tcp.accept();
    msg_tcp.download(path);
    /* TODO if error download then acquire mutex and available space += file size and erase files[f] */
  } catch (...) {
    std::cout << "accepting or downloading failed " << std::endl;
    std::unique_lock lk(m);
    inc_space(fsize);
    files.erase(f);
  }
}

void server::run() {
  /* TODO endless loop */
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
      list(remote_address, simple.cmd_seq(), simple.data);
    } else if (cmd::eq(simple.cmd, cmd::get)) {
      get(remote_address, simple.cmd_seq(), simple.data);
    } else if (cmd::eq(simple.cmd, cmd::del)) {
      del(simple.data);
    } else if (cmd::eq(simple.cmd, cmd::add)) {
      cmd::complex complex(&simple);
      add(remote_address, complex.cmd_seq(), complex.param(), complex.data);
    } else {
      msg::skipping(inet_ntoa(remote_address.sin_addr), ntohs(remote_address.sin_port));
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
    s.run();
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    exit(EXIT_FAILURE);
  }
}
