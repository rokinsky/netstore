#ifndef _HELPER_HH
#define _HELPER_HH

#include <boost/serialization/string.hpp>
#include <boost/serialization/access.hpp>

namespace netstore {

constexpr size_t max_udp = 65507;

} // namespace netstore

namespace netstore::cmd {

constexpr size_t max_cmd = 10;
constexpr size_t max_simpl_data = max_udp - max_cmd - sizeof(uint64_t);
constexpr size_t max_cmlpx_data = max_udp - max_cmd - 2 * sizeof(uint64_t);

/* Rozpoznawanie listy serwerów w grupie */
constexpr char hello[] = "HELLO"; /* client, simpl */
constexpr char good_day[] ="GOOD_DAY"; /* server, cmplx[free space, MCAST_ADDR] */

/* Przeglądanie listy plików i wyszukiwanie na serwerach w grupie */
constexpr char list[] = "LIST"; /* client, simpl[if data is not empty => search file] */
constexpr char my_list[] = "MY_LIST"; /* server, simpl[filelist] */

/* Pobieranie pliku z serwera */
constexpr char get[] = "GET"; /* client, simpl[filename] */
constexpr char connect_me[] = "CONNECT_ME"; /* server, cmplx[port TCP, filename] */

/* Usuwanie pliku z serwera */
constexpr char del[] = "DEL"; /* client, simpl[filename] */

/* Dodawanie pliku do grupy */
constexpr char add[] = "ADD"; /* client, cmplx[size, filename] */
constexpr char no_way[] = "NO_WAY"; /* server, simpl[filename] */
constexpr char can_add[] = "CAN_ADD"; /* server, cmplx[port TCP, {}] */


struct __attribute__ ((packed)) simple {
  char cmd[max_cmd] = {0};
  uint64_t cmd_seq = 0;
  char data[max_simpl_data] = {0};

  simple(const char *cmd_, uint64_t cmd_seq_, const char *data_) {
    strncpy(cmd, cmd_, max_cmd);
    cmd_seq = cmd_seq_;
    strncpy(data, data_, max_simpl_data);
  }

  simple() = default;

  size_t set_data(const char *d) {
    strncpy(data, d, max_simpl_data);
    return std::min<size_t>(max_simpl_data, strlen(d));
  }

  size_t data_size() {
    return strlen(data);
  }

  size_t size() {
    return sizeof(cmd) + sizeof(cmd_seq) + strlen(data);
  }
};

struct __attribute__ ((packed)) complex {
  char cmd[max_cmd];
  uint64_t cmd_seq;
  uint64_t param;
  char data[max_cmlpx_data];
};

} // namespace netstore::cmd

#endif // _HELPER_HH
