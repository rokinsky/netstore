#ifndef _AUX_HH
#define _AUX_HH

#include <string>
#include <boost/filesystem.hpp>
#include <chrono>

namespace netstore::aux {

bool is_discover(const std::string& s);

bool is_search(const std::string& s, std::string& param);

bool is_fetch(const std::string& s, std::string& param);

bool is_upload(const std::string& s, std::string& param);

bool is_remove(const std::string& s, std::string& param);

bool is_exit(const std::string& s);

void check_dir(const std::string& s, const std::string& param);

template <typename T>
void check_range(T value, T min, T max, const std::string& param) {
  namespace bpo = boost::program_options;
  if (value < min || value > max) {
    throw bpo::validation_error(
        bpo::validation_error::invalid_option_value, param, std::to_string(value));
  }
}

inline std::string path(const std::string& dir, const std::string& filename) {
  return dir + '/' + filename;
}

inline bool validate(const std::string& filename) {
  return !filename.empty() && filename.find('/', 0) == std::string::npos;
}

inline bool exists(const std::string& path) {
  return boost::filesystem::exists(path);
}

template <typename Duration>
struct timeval to_timeval(Duration&& d) {
  const auto sec = std::chrono::duration_cast<std::chrono::seconds>(d);
  struct timeval tv {};
  tv.tv_sec = sec.count();
  tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(d - sec).count();
  return tv;
}

} // namespace netstore::aux

#endif // _AUX_HH
