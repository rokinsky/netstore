#ifndef _HELPER_HH
#define _HELPER_HH

#include <boost/serialization/string.hpp>
#include <boost/serialization/access.hpp>
#include <filesystem>
#include <cstring>
#include <boost/program_options.hpp>

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

#endif // _HELPER_HH
