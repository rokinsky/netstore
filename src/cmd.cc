#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include "cmd.hh"
#include "common.hh"

namespace netstore::cmd {

  simple::simple(const char *cmd_, uint64_t cmd_seq_, const char *data_, size_t data_size) {
    strncpy(cmd, cmd_, max_cmd);
    _cmd_seq = htobe64(cmd_seq_);
    strncpy(data, data_, data_size);
  }

  simple::simple(complex* complex) {
    memmove(this, complex, netstore::max_udp);
  }

  size_t simple::set_data(const char *d) {
    strncpy(data, d, max_simpl_data);
    return std::min<size_t>(max_simpl_data, std::strlen(d));
  }


  std::string complex::to_string() {
    return std::string(std::string(cmd) + " "
                       + std::to_string(be64toh(_cmd_seq)) + " "
                       + std::to_string(be64toh(_param)) + " "
                       + std::string(data));
  }

  complex::complex(simple* simple) {
    memmove(this, simple, max_udp);
  }

  complex::complex(const char* cmd_, uint64_t cmd_seq_, uint64_t param_, const char* data_) {
    strncpy(cmd, cmd_, max_cmd);
    _cmd_seq = htobe64(cmd_seq_);
    _param = htobe64(param_);
    strncpy(data, data_, max_simpl_data);
  }
}