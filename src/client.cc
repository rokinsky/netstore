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
#include <unordered_map>
#include <vector>
#include <tuple>
#include <mutex>

#include <boost/bind.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "common.hh"
#include "sockets.hh"
#include "aux.hh"
#include "cmd.hh"

namespace netstore {

class client {
 public:
   client(std::string ma, in_port_t cp, std::string of, uint8_t t)
    : mcast_addr(std::move(ma))
    , cmd_port(cp)
    , mcast_sockaddr(set_target(mcast_addr, cmd_port))
    , out_fldr(std::move(of))
    , timeout(t)
    , udp(0)
   { connect(udp); }

   void connect(sockets::udp& sock);
   void run();

 private:
  /* <available memory, multicast address, unicast address> */
  typedef std::tuple<uint64_t, std::string, std::string> mmu_t;

  sockaddr_in set_target(const std::string& addr, in_port_t port);

  std::vector<mmu_t> discover(sockets::udp& sock);

  void discover();

  void search(const std::string& pattern);

  void fetch(const std::string& param);

  void upload(const std::string& param);

  void remove(const std::string& param);

  static uint64_t cmd_seq();

  std::string mcast_addr;
  in_port_t cmd_port;
  sockaddr_in mcast_sockaddr;
  std::string out_fldr;
  uint8_t timeout;
  sockets::udp udp;
  std::unordered_map<std::string, std::string> files;
  std::mutex m;

  static constexpr uint8_t ttl_value = 4;
};

uint64_t client::cmd_seq() {
  static thread_local uint64_t cmd_seq = 0;
  return ++cmd_seq;
}

std::vector<client::mmu_t> client::discover(sockets::udp& sock) {
  std::vector<mmu_t> servers;
  printf("Sending request...\n");
  cmd::simple simple { cmd::hello, cmd_seq() };

  sock.send(simple, mcast_sockaddr);

  sock.do_until(timeout, [&] {
    printf("Waiting for response...\n");
    cmd::complex complex;
    sockaddr_in ra{};
    if (sock.recv(complex, ra) > 0
    && cmd::validate(complex, simple, cmd::good_day)
    && !complex.is_empty_data())
      servers.emplace_back(std::make_tuple(complex.param(),
                                           std::string(complex.data),
                                           std::string(inet_ntoa(ra.sin_addr))));
  });
  printf("End timeout.\n");

  std::sort(servers.begin(), servers.end());

  return servers;
}

void client::discover() {
  for (const auto& [mem, mcast, ucast]: discover(udp))
    msg::found(ucast, mcast, mem);
}

void client::search(const std::string& pattern) {
  printf("Sending request...\n");
  files.clear();
  cmd::simple simple { cmd::list, cmd_seq(), pattern.data() };

  udp.send(simple, mcast_sockaddr);

  udp.do_until (timeout, [&] {
    printf("Waiting for response...\n");
    cmd::simple rcved;
    sockaddr_in ra{};
    if (udp.recv(rcved, ra) > 0) {
      printf("address: %s:%d\n", inet_ntoa(ra.sin_addr),
             ntohs(ra.sin_port));
      if (cmd::validate(rcved, simple, cmd::my_list)) {
        std::string list(rcved.data);
        size_t offset = 0;
        size_t N = list.length();
        while (offset < N) {
          size_t start = offset;
          offset = list.find('\n', start) + 1;
          size_t end = offset - 1;
          if (offset - 1 == std::string::npos) {
            end = offset = N;
          }
          std::string filename(rcved.data + start, rcved.data + end);
          files[filename] = std::string(inet_ntoa(ra.sin_addr));
          msg::searched(filename, inet_ntoa(ra.sin_addr));
        }
      } else {
        // TODO error
      }
    }
  });
  printf("Timeout end\n");
}

void client::fetch(const std::string& param) {
  std::string addr;
  {
    std::unique_lock lk(m);
    addr = files[param];
  }

  if (addr.empty()) {
    std::cout << "nie ma pliku" << std::endl;
    return;
  }

  printf("Sending request...\n");

  auto server_address = set_target(addr, cmd_port);

  cmd::simple simple { cmd::get, cmd_seq(), param.data() };
  udp.send(simple, server_address);

  printf("Waiting for response...\n");
  cmd::complex complex;

  sockaddr_in receive_address{};
  ssize_t rcv_len = udp.recv(complex, receive_address);

  if (rcv_len >= 0 && cmd::validate(complex, simple, cmd::connect_me)) {
    std::cout << "Connect_me " << inet_ntoa(receive_address.sin_addr) << ":"
              << complex.param() << " (" << complex.data << ")" << std::endl;
    try {
      sockets::tcp tcp;
      tcp.set_timeout({timeout, 0});
      tcp.connect(files[param], complex.param());
      tcp.download(aux::path(out_fldr, complex.data));
      msg::downloaded(param, inet_ntoa(receive_address.sin_addr), complex.param());
    } catch (...) {
      msg::downloading_failed(param, inet_ntoa(receive_address.sin_addr), complex.param(), "");
    }
  } else {
    msg::downloading_failed(param, "", 0, "");
    std::cout << "blad " << std::strerror(errno) << std::endl;
  }
}

void client::upload(const std::string& param) {
  if (!aux::exists(param) || !std::filesystem::is_regular_file(param)) {
    msg::not_exists(param);
    return;
  }

  sockets::udp udp_msg(0);
  connect(udp_msg);

  auto uploaded = false;
  auto servers = discover(udp_msg);
  while (!servers.empty() && !uploaded) {
    const auto& [_, mcast, ucast] = servers.back();
    servers.pop_back();
    cmd::complex cmplx_snd { cmd::add, cmd_seq(),
                         std::filesystem::file_size(param),
                         std::filesystem::path(param).filename().c_str()
                         };
    printf("UPLOAD: Sending request\n");
    auto sockaddr = set_target(ucast, cmd_port);
    udp_msg.send(cmplx_snd, sockaddr);
    printf("UPLOAD: Waiting for response...\n");

    cmd::simple simple {};
    udp_msg.recv(simple, sockaddr);

    if (cmd::validate(simple, cmplx_snd, cmd::no_way) && simple.data == param) {
      std::cout << "NO_WAY" << std::endl;
      continue;
    } else if (cmd::validate(simple, cmplx_snd, cmd::can_add)) {
      cmd::complex cmplx_rcv(&simple);
      if (cmplx_rcv.is_empty_data() && ucast == inet_ntoa(sockaddr.sin_addr)) {
        std::cout << "Connect_me " << inet_ntoa(sockaddr.sin_addr) << ":"
        << cmplx_rcv.param() << " (" << cmplx_rcv.data << ")" << std::endl;
        try {
          sockets::tcp tcp;
          tcp.set_timeout({timeout, 0});
          tcp.connect(ucast, cmplx_rcv.param());
          tcp.upload(param);
          uploaded = true;
          msg::uploaded(param, ucast, cmplx_rcv.param());
        } catch (...) {
          msg::uploading_failed(param, ucast, cmplx_rcv.param(), "");
        }
        continue;
      }
    }
    msg::skipping(inet_ntoa(sockaddr.sin_addr), sockaddr.sin_port);
  }

  if (!uploaded) {
    msg::too_big(param);
  } else {

  }
}

void client::remove(const std::string& param) {
  cmd::simple simple { cmd::del, cmd_seq(), param.data() };
  if (param.empty()) throw std::logic_error("filename is empty");
  udp.send(simple, mcast_sockaddr);
}

void client::connect(sockets::udp& sock) {
  sock.set_broadcast();
  sock.set_ttl(ttl_value);
  sock.set_timeout(timeval{timeout, 0});
}

void client::run() {
  std::string line;
  std::getline(std::cin, line);

  while(!aux::is_exit(line)) {
    std::string param;
    if (aux::is_discover(line)) {
      std::cout << "!!discover" << std::endl;
      discover();
    } else if (aux::is_search(line, param)) {
      std::cout << "!!searched word: " << param << std::endl;
      search(param);
    } else if (aux::is_fetch(line, param)) {
      std::cout << "!!fetch" << std::endl;
      fetch(param);
    } else if (aux::is_upload(line, param)) {
      std::cout << "!!upload" << std::endl;
      upload(param);
    } else if (aux::is_remove(line, param)) {
      std::cout << "!!remove" << std::endl;
      remove(param);
    }
    std::cout << line << std::endl;
    std::getline(std::cin, line);
  }
  printf("Closing.\n");
}

sockaddr_in client::set_target(const std::string& addr, in_port_t port) {
  sockaddr_in sockaddr{};
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port);
  if (inet_aton(addr.c_str(), &sockaddr.sin_addr) == 0)
    throw std::runtime_error("inet_aton");

  return sockaddr;
}

} // namespace netstore

int main(int ac, char** av) {
  namespace bpo = boost::program_options;
  std::string mcast_addr;
  int64_t cmd_port;
  std::string out_fldr;
  int64_t timeout;

  bpo::options_description desc("Allowed options");
  desc.add_options()
    (",g", bpo::value(&mcast_addr)->required(), "Multicast address")
    (",p", bpo::value(&cmd_port)->required()->notifier(
        boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, std::numeric_limits<uint16_t>::max(), "p")), "UDP port")
    (",o", bpo::value(&out_fldr)->required()->notifier(boost::bind(&netstore::aux::check_dir, _1)),"Output folder")
    (",t", bpo::value(&timeout)->default_value(5)->notifier(boost::bind(&netstore::aux::check_range<int64_t>, _1, 0, 300, "t")), "Timeout")
  ;

  try {
    bpo::variables_map vm;
    store(bpo::parse_command_line(ac, av, desc), vm);
    notify(vm);

    netstore::client c(mcast_addr, cmd_port, out_fldr, timeout);
    c.run();
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    exit(EXIT_FAILURE);
  }
}
