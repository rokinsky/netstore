#ifndef _HELPER_HH
#define _HELPER_HH

#include <boost/serialization/string.hpp>
#include <boost/serialization/access.hpp>
#include <filesystem>

namespace netstore {

class exception : public std::exception {
  std::string msg;

 public:
  explicit exception(const std::string& msg)
    : msg(msg + "(" + std::strerror(errno) + ")")
    {}

  const char* what() const noexcept final {
    return msg.c_str();
  }
};

constexpr size_t max_udp = 65507;

} // namespace netstore

namespace netstore::aux {
template<typename T>
void check_range(const T& value, const T& min, const T& max, const std::string& param) {
  namespace bpo = boost::program_options;
  if (value < min || value > max) {
    throw bpo::validation_error(
        bpo::validation_error::invalid_option_value, param, std::to_string(value)
    );
  }
}

void check_dir(const std::string& s) {
  namespace bpo = boost::program_options;
  if (not std::filesystem::is_directory(s))
    throw bpo::validation_error(
        bpo::validation_error::invalid_option_value, "f", s
    );
}
}

namespace netstore::cmd {

constexpr size_t max_cmd = 10;
constexpr size_t max_simpl_data = max_udp - max_cmd - sizeof(uint64_t);
constexpr size_t max_cmlpx_data = max_udp - max_cmd - 2 * sizeof(uint64_t);

inline bool eq(const char* a, const char* b) {
  return strncmp(a, b, max_cmd) == 0;
}

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

struct complex;
struct simple;

struct __attribute__ ((packed)) simple {
  char cmd[max_cmd] = {0};
  uint64_t cmd_seq = 0;
  char data[max_simpl_data] = {0};

  simple(const char *cmd_, uint64_t cmd_seq_, const char *data_ = "") {
    strncpy(cmd, cmd_, max_cmd);
    cmd_seq = cmd_seq_;
    strncpy(data, data_, max_simpl_data);
  }

  explicit simple(complex* complex) {
    memmove(this, complex, max_udp);
  }

  simple() = default;

  size_t set_data(const char *d) {
    strncpy(data, d, max_simpl_data);
    return std::min<size_t>(max_simpl_data, strlen(d));
  }

  inline size_t data_size() {
    return strlen(data);
  }

  size_t size() {
    return sizeof(cmd) + sizeof(cmd_seq) + strlen(data);
  }

  inline bool is_empty_data() {
    return data_size() == 0;
  }
};

struct __attribute__ ((packed)) complex {
  char cmd[max_cmd] = {0};
  uint64_t cmd_seq = 0;
  uint64_t param = 0;
  char data[max_cmlpx_data] = {0};

  complex() = default;

  complex(const char* cmd_, uint64_t cmd_seq_, uint64_t param_, const char* data_ = "") {
    strncpy(cmd, cmd_, max_cmd);
    cmd_seq = htobe64(cmd_seq_);
    param = htobe64(param_);
    strncpy(data, data_, max_simpl_data);
  }

  explicit complex(simple* simple) {
    memmove(this, simple, max_udp);
  }

  std::string to_string() {
    return std::string(std::string(cmd) + " "
    + std::to_string(be64toh(cmd_seq)) + " "
    + std::to_string(be64toh(param)) + " "
    + std::string(data));
  }

  size_t size() {
    return sizeof(cmd) + sizeof(cmd_seq) + sizeof(param) + strlen(data);
  }
};

} // namespace netstore::cmd

#endif // _HELPER_HH
