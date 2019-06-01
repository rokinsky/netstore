#ifndef NETSTORE_2_CMD_HH
#define NETSTORE_2_CMD_HH

#include "common.hh"

namespace netstore::cmd {

  constexpr size_t max_cmd = 10;
  constexpr size_t max_simpl_data = max_udp - max_cmd - sizeof(uint64_t);
  constexpr size_t max_cmlpx_data = max_udp - max_cmd - 2 * sizeof(uint64_t);

  inline bool eq(const char* a, const char* b) {
    return strncmp(a, b, max_cmd) == 0; // TODO: memcpy !!!
  }

  template <typename A, typename B>
  inline bool validate(A& rcv, B& snd, const std::string& expected) {
    return eq(rcv.cmd, expected.c_str()) && rcv.cmd_seq() == snd.cmd_seq();
  }

  /* Rozpoznawanie listy serwerów w grupie */
  constexpr char hello[] = "HELLO"; /* client, simpl */
  constexpr char good_day[] ="GOOD_DAY"; /* server, cmplx[free space, mcast_addr] */

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

  struct complex;
  struct simple;

  struct __attribute__ ((packed)) simple {
    char cmd[max_cmd] = {0};
    uint64_t _cmd_seq = 0;
    char data[max_simpl_data] = {0};

    simple(const char *cmd_, uint64_t cmd_seq_, const char *data_ = "", size_t data_size = max_simpl_data);

    explicit simple(complex* complex);

    simple() = default;

    inline uint64_t cmd_seq() {
      return be64toh(_cmd_seq);
    }

    size_t set_data(const char *d);

    inline size_t data_size() {
      return strlen(data);
    }

    inline ssize_t size() {
      return sizeof(cmd) + sizeof(_cmd_seq) + strlen(data);
    }

    inline bool is_empty_data() {
      return data_size() == 0;
    }
  };

  struct __attribute__ ((packed)) complex {
    char cmd[max_cmd] = {0};
    uint64_t _cmd_seq = 0;
    uint64_t _param = 0;
    char data[max_cmlpx_data] = {0};

    complex() = default;

    complex(const char* cmd_, uint64_t cmd_seq_, uint64_t param_, const char* data_ = "");

    explicit complex(simple* simple);

    std::string to_string();

    inline uint64_t cmd_seq() {
      return be64toh(_cmd_seq);
    }

    inline uint64_t param() {
      return be64toh(_param);
    }

    inline ssize_t size() {
      return sizeof(cmd) + sizeof(_cmd_seq) + sizeof(_param) + strlen(data);
    }
  };

} // namespace netstore::cmd

#endif //NETSTORE_2_CMD_HH
