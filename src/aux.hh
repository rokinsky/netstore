#ifndef NETSTORE_2_AUX_HH
#define NETSTORE_2_AUX_HH

#include <string>

namespace netstore::aux {
  bool is_discover(const std::string& s);

  bool is_search(const std::string& s, std::string& param);

  bool is_fetch(const std::string& s, std::string& param);

  bool is_upload(const std::string& s, std::string& param);

  bool is_remove(const std::string& s, std::string& param);

  bool is_exit(const std::string& s);

  void check_dir(const std::string& s);

  template<typename T>
  void check_range(T value, T min, T max, const std::string& param) {
    namespace bpo = boost::program_options;
    if (value < min || value > max) {
      throw bpo::validation_error(
          bpo::validation_error::invalid_option_value, param, std::to_string(value)
      );
    }
  }

  inline std::string path(const std::string& dir, const std::string& filename) {
    return dir + '/' + filename;
  }

  inline bool validate(const std::string& filename) {
    return !filename.empty() && filename.find('/', 0) == std::string::npos;
  }

  inline bool exists(const std::string& path) {
    return std::filesystem::exists(path);
  }
}

#endif //NETSTORE_2_AUX_HH
