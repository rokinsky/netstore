/* TODO timeout
 * Maciej Bala Jak mamy watek o sieciach, to ja mam pytanie. Jak wykonujecie czekanie TIMEOUT sekund w poleceniu discover? Select albo poll maja opcje timeoutu, ale blokuja tylko do przyjscia pierwszego pakietu, ktory rownie dobrze moze przyjsc prawie od razu. Ja poki co uzywam do tego sleepa po prostu, ale nie wiem czy o to chodzi. Ta sama kwestia pojawia sie w sumie w poleceniu search
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
Maciej Bala Czyli jak dobrze rozumiem, to wywolanie polla na tym fd tak samo zablokuje proces jak sleep? Czy da sie jakos wykryc i przerobic wczesniejsze komunikaty, jeszcze przed uplywem TIMEOUT sekund?
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

    socklen_t remote_len = sizeof(struct sockaddr_in);
    ssize_t rcv_len = 0;
    struct sockaddr_in ra{};

    while (rcv_len >= 0) {
      printf("Waiting for response...\n");
      cmd::complex complex;
      rcv_len = recvfrom(sock, &complex, sizeof(complex), 0, (struct sockaddr*) &ra, &remote_len);
      if (rcv_len >= 0 && cmd::eq(complex.cmd, cmd::good_day) && complex.cmd_seq() == simple.cmd_seq()) {
        std::cout << "Found " << inet_ntoa(ra.sin_addr) << " (" << complex.data << ") with free space " << complex.param() << std::endl;
        //std::cout << complex.to_string() << std::endl;
      }
    }
    printf("Didn't get any response. Break request.\n");
  }

  void search(const std::string& pattern) {
    printf("Sending request...\n");
    files.clear();
    cmd::simple simple { cmd::list, 10, pattern.data() };
    if (sendto(sock, &simple, simple.size(), 0, (struct sockaddr*) &remote_address, sizeof(remote_address)) != simple.size())
      throw std::runtime_error("write");

    socklen_t remote_len = sizeof(struct sockaddr_in);
    ssize_t rcv_len = 0;
    struct sockaddr_in ra{};

    while (rcv_len >= 0) {
      printf("Waiting for response...\n");
      memset(&simple, 0, sizeof(simple));
      rcv_len = recvfrom(sock, &simple, sizeof(simple), 0, (struct sockaddr *) &ra, &remote_len);
      if (rcv_len >= 0) {
        printf("address: %s:%d\n", inet_ntoa(ra.sin_addr),
               ntohs(ra.sin_port));
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

  void fetch(const std::string& param) {
    if (files.find(param) != files.end()) {
      auto server_address = remote_address;
      if (inet_aton(files[param].c_str(), &server_address.sin_addr) == 0)
        throw std::runtime_error("inet_aton");

      printf("Sending request...\n");
      cmd::simple simple { cmd::get, 10, param.data() };
      if (sendto(sock, &simple, simple.size(), 0, (struct sockaddr*) &server_address, sizeof(server_address)) != simple.size())
        throw std::runtime_error("write");

      socklen_t remote_len;
      ssize_t rcv_len = 0;
      printf("Waiting for response...\n");
      cmd::complex complex;
      rcv_len = recvfrom(sock, &complex, sizeof(complex), 0, (struct sockaddr*) &server_address, &remote_len);
      if (rcv_len >= 0 && cmd::eq(complex.cmd, cmd::connect_me) && complex.cmd_seq() == simple.cmd_seq()) {
        std::cout << "Connect_me " << inet_ntoa(server_address.sin_addr) << ":" << complex.param() << " (" << complex.data << ")" << std::endl;
      } else {
        std::cout << "blad" << std::endl;
      }
    } else {
      std::cout << "nie ma pliku" << std::endl;
    }
  }

  std::string remote_dotted_address;
  in_port_t remote_port;
  int sock;
  struct sockaddr_in remote_address;
  std::unordered_map<std::string, std::string> files;


  inline bool is_discover(const std::string& s) {
    return s == "discover";
  }

  inline bool is_search(const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^search( |( (.*))?)"));

    if (is_match)
      param = s.substr(std::min<size_t>(strlen("search "), s.length()));

    return is_match;
  }

  inline bool is_fetch(const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^fetch( (.+)){1}"));

    if (is_match)
      param = s.substr(std::min<size_t>(strlen("fetch "), s.length()));

    return is_match;
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
    } else if (is_fetch(line, param)) {
      std::cout << "!!fetch" << std::endl;
      fetch(param);
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
  struct sockaddr_in local_address{};
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
  struct timeval tv {5, 0};
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
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
