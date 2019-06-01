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
#include <boost/bind.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "common.hh"
#include "sockets.hh"
#include "aux.hh"
#include "cmd.hh"

#define BSIZE         256
#define TTL_VALUE     4
#define REPEAT_COUNT  3
#define SLEEP_TIME    5

namespace netstore {

class client {
 public:
   client(std::string mcast_addr, in_port_t cmd_port, std::string of, uint8_t t)
    : mcast_addr(std::move(mcast_addr))
    , cmd_port(cmd_port)
    , remote_address({})
    , out_fldr(std::move(of))
    , timeout(t)
    , udp(0)
   {}

   ~client() {
     udp.unset_multicast();
   }

   void connect();
   void run();

 private:
  void set_target();

  void discover();

  void search(const std::string& pattern);

  void fetch(const std::string& param);

  std::string mcast_addr;
  in_port_t cmd_port;
  struct sockaddr_in remote_address;
  std::string out_fldr;
  uint8_t timeout;
  sockets::udp udp;
  std::unordered_map<std::string, std::string> files;
};

void client::discover() {
  printf("Sending request...\n");
  cmd::simple simple { cmd::hello, 10 };

  udp.send(simple, remote_address);

  ssize_t rcv_len = 0;
  struct sockaddr_in ra{};

  while (rcv_len >= 0) {
    printf("Waiting for response...\n");
    cmd::complex complex;
    rcv_len = udp.recv(complex, ra);
    if (rcv_len >= 0 && cmd::validate(complex, simple, cmd::good_day)) {
      std::cout << "Found " << inet_ntoa(ra.sin_addr) << " (" << complex.data << ") with free space " << complex.param() << std::endl;
    }
  }
  printf("Didn't get any response. Break request.\n");
}

void client::search(const std::string& pattern) {
  printf("Sending request...\n");
  files.clear();
  cmd::simple simple { cmd::list, 10, pattern.data() };

  udp.send(simple, remote_address);

  ssize_t rcv_len = 0;
  struct sockaddr_in ra{};

  while (rcv_len >= 0) {
    printf("Waiting for response...\n");
    cmd::simple rcved;
    rcv_len = udp.recv(rcved, ra);
    if (rcv_len >= 0) {
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
          std::cout << filename << " (" << inet_ntoa(ra.sin_addr) << ")" << std::endl;
        }
      } else {
        // TODO error
      }
    }
  }
  printf("Didn't get any response. Break request.\n");
}

void client::fetch(const std::string& param) {
  if (files.find(param) != files.end()) {
    auto server_address = remote_address;
    if (inet_aton(files[param].c_str(), &server_address.sin_addr) == 0)
      throw std::runtime_error("inet_aton");

    printf("Sending request...\n");
    cmd::simple simple { cmd::get, 10, param.data() };
    udp.send(simple, server_address);

    ssize_t rcv_len = 0;
    printf("Waiting for response...\n");
    cmd::complex complex;
    rcv_len = udp.recv(complex, server_address);

    if (rcv_len >= 0 && cmd::validate(complex, simple, cmd::connect_me)) {
      std::cout << "Connect_me " << inet_ntoa(server_address.sin_addr) << ":" << complex.param() << " (" << complex.data << ")" << std::endl;
    } else {
      std::cout << "blad" << std::endl;
    }
  } else {
    std::cout << "nie ma pliku" << std::endl;
  }
}

void client::connect() {
  udp.set_broadcast();
  udp.set_ttl(TTL_VALUE);
  udp.set_timeout(SLEEP_TIME, 0);

  set_target();
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
    } else if (aux::is_upload(line)) {
      std::cout << "!!upload" << std::endl;
    } else if (aux::is_remove(line)) {
      std::cout << "!!remove" << std::endl;
    }
    std::cout << line << std::endl;
    std::getline(std::cin, line);
  }
  printf("Closing.\n");
}

void client::set_target() {
  remote_address.sin_family = AF_INET;
  remote_address.sin_port = htons(cmd_port);
  if (inet_aton(mcast_addr.c_str(), &remote_address.sin_addr) == 0)
    throw std::runtime_error("inet_aton");
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
