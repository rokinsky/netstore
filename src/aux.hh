#ifndef NETSTORE_2_AUX_HH
#define NETSTORE_2_AUX_HH

#include <string>

namespace netstore::aux {
  bool is_discover(const std::string& s);

  bool is_search(const std::string& s, std::string& param);

  bool is_fetch(const std::string& s, std::string& param);

  bool is_upload(const std::string& s);

  bool is_remove(const std::string& s);

  bool is_exit(const std::string& s);

  void check_dir(const std::string& s);

  template<typename T>
  void check_range(const T& value, const T& min, const T& max, const std::string& param) {
    namespace bpo = boost::program_options;
    if (value < min || value > max) {
      throw bpo::validation_error(
          bpo::validation_error::invalid_option_value, param, std::to_string(value)
      );
    }
  }
}

#endif //NETSTORE_2_AUX_HH
