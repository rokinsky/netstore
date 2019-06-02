/* TODO timeout
 * Maciej Bala Jak mamy watek o sieciach, to ja mam pytanie. Jak wykonujecie czekanie timeout sekund w poleceniu discover? Select albo poll maja opcje timeoutu, ale blokuja tylko do przyjscia pierwszego pakietu, ktory rownie dobrze moze przyjsc prawie od razu. Ja poki co uzywam do tego sleepa po prostu, ale nie wiem czy o to chodzi. Ta sama kwestia pojawia sie w sumie w poleceniu search
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d · Edytowano
Kuba Martin
Kuba Martin timerfd_create
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
Kuba Martin
Kuba Martin Daje Ci fd na którym możesz pollować
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
Maciej Bala
Maciej Bala Czyli jak dobrze rozumiem, to wywolanie polla na tym fd tak samo zablokuje proces jak sleep? Czy da sie jakos wykryc i przerobic wczesniejsze komunikaty, jeszcze przed uplywem timeout sekund?
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
Kuba Martin
Kuba Martin Poll przyjmuje tablicę pollfd, więc jak mu dasz pollfd timerów i socketów to zwróci Ci pierwszy z którym będzie coś do zrobienia.
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
Filip Mikina
Filip Mikina Maciej Bala
Ukryj lub zgłoś
Brak dostępnego opisu zdjęcia.
1
Lubię to!
 · Odpowiedz · 2 d
Filip Mikina
Filip Mikina mam nadzieje ze sie kod sam dokumentuje
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
Maciej Bala
Maciej Bala Filip Mikina A co robi fill_timeout i set_read_timeout?
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
Filip Mikina
Filip Mikina Maciej Bala fill_timeout ustawia read_timeout na timeout - tyle_ile_czasu_uplynelo, set_read_timeout ustawia read timeout socketu na read_timeout
Ukryj lub zgłoś
Lubię to!
 · Odpowiedz · 2 d
*/


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

#define TTL_VALUE     4
#define REPEAT_COUNT  3

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
   {}

   void connect();
   void run();

 private:
  /* memo, mcast_addr, ucast_addr */
  typedef std::tuple<uint64_t, std::string, std::string> mmu_t;

  sockaddr_in set_target(const std::string& addr, in_port_t port);

  std::vector<mmu_t> discover();

  static void print_servers(const std::vector<mmu_t>& servers);

  void search(const std::string& pattern);

  void fetch(const std::string& param);

  void upload(const std::string& param);

  static uint64_t cmd_seq();

  std::string mcast_addr;
  in_port_t cmd_port;
  sockaddr_in mcast_sockaddr;
  std::string out_fldr;
  uint8_t timeout;
  sockets::udp udp;
  std::unordered_map<std::string, std::string> files;
  std::mutex m;
};

uint64_t client::cmd_seq() {
  static thread_local uint64_t cmd_seq = 0;
  return ++cmd_seq;
}

std::vector<client::mmu_t> client::discover() {
  std::vector<mmu_t> servers;
  printf("Sending request...\n");
  cmd::simple simple { cmd::hello, cmd_seq() };

  udp.send(simple, mcast_sockaddr);

  udp.do_until(timeout, [&] {
    printf("Waiting for response...\n");
    cmd::complex complex;
    sockaddr_in ra{};
    if (udp.recv(complex, ra) > 0 && cmd::validate(complex, simple, cmd::good_day))
      servers.emplace_back(std::make_tuple(complex.param(),
                                           std::string(complex.data),
                                           std::string(inet_ntoa(ra.sin_addr))));
  });
  printf("End timeout.\n");

  std::sort(servers.begin(), servers.end());

  return servers;
}

void client::print_servers(const std::vector<mmu_t>& servers) {
  for (const auto& [mem, mcast, ucast]: servers) msg::found(ucast, mcast, mem);
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

  ssize_t rcv_len = udp.recv(complex, server_address);

  if (rcv_len >= 0 && cmd::validate(complex, simple, cmd::connect_me)) {
    std::cout << "Connect_me " << inet_ntoa(server_address.sin_addr) << ":" << complex.param() << " (" << complex.data << ")" << std::endl;

    sockets::tcp tcp;
    tcp.connect(files[param], complex.param());
    tcp.download(aux::path(out_fldr, complex.data));
  } else {
    std::cout << "blad " << std::strerror(errno) << std::endl;
  }
}

void client::upload(const std::string& param) {
  if (!aux::exists(param)) {
    msg::not_exists(param);
    return;
  }

  auto servers = discover();
  print_servers(servers);
  auto uploaded = false;

  sockets::udp udp_msg(0);
  udp_msg.set_ttl(TTL_VALUE);
  udp_msg.set_timeout(timeval{timeout, 0});

  while (!servers.empty() && !uploaded) {
    const auto& [mem, mcast, ucast] = servers.back();
    servers.pop_back();
    auto sockaddr = set_target(ucast, cmd_port);
    cmd::complex cmplx_snd { cmd::add, cmd_seq(),
                         std::filesystem::file_size(param),
                         std::filesystem::path(param).filename().c_str()
                         };
    printf("UPLOAD: Sending request\n");
    udp_msg.send(cmplx_snd, sockaddr);
    printf("UPLOAD: Waiting for response...\n");
    cmd::simple simple {};
    udp_msg.recv(simple, sockaddr);
    if (cmd::validate(simple, cmplx_snd, cmd::no_way) && simple.is_empty_data()) {
      continue;
    } else if (cmd::validate(simple, cmplx_snd, cmd::can_add)) {
      cmd::complex cmplx_rcv(&simple);
      if (cmplx_rcv.is_empty_data() && ucast == inet_ntoa(sockaddr.sin_addr)) {
        std::cout << "Connect_me " << inet_ntoa(sockaddr.sin_addr) << ":" << cmplx_rcv.param() << " (" << cmplx_rcv.data << ")" << std::endl;
        sockets::tcp tcp;
        tcp.connect(ucast, cmplx_rcv.param());
        tcp.upload(param);
        uploaded = true;
        msg::uploaded(param, inet_ntoa(sockaddr.sin_addr), sockaddr.sin_port);
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

//client::~client();

void client::connect() {
  udp.set_broadcast();
  udp.set_ttl(TTL_VALUE);
  udp.set_timeout(timeval{timeout, 0});
}

void client::run() {

  std::string line;
  std::getline(std::cin, line);

  while(!aux::is_exit(line)) {
    std::string param;
    if (aux::is_discover(line)) {
      std::cout << "!!discover" << std::endl;
      print_servers(discover());
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
    c.connect();
    c.run();
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    exit(EXIT_FAILURE);
  }
}
