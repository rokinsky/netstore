#ifndef _HELPER_HH
#define _HELPER_HH

#include <boost/serialization/string.hpp>
#include <boost/serialization/access.hpp>
#include <filesystem>
#include <string>
#include <boost/program_options.hpp>
#include <iostream>
#include <netinet/in.h>

namespace netstore {

static std::atomic<bool> quit{false};
void handler(int _) {
  std::cout << "SIGINT handled" << std::endl;
  quit.store(true);
}

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

namespace netstore::msg {
  inline void skipping(const std::string& addr, in_port_t port) {
    std::cout << "[PCKG ERROR]  Skipping invalid package from " << addr << ":" << port << "." << std::endl;
  }

  inline void uploaded(const std::string& file, const std::string& addr, in_port_t port) {
    std::cout << "File " << file << " uploaded ("<< addr << ":" << port << ")" << std::endl;
  }

  inline void downloaded(const std::string& file, const std::string& addr, in_port_t port) {
    std::cout << "File " << file << " downloaded ("<< addr << ":" << port << ")" << std::endl;
  }

  inline void found(const std::string& ucast, const std::string& mcast, uint64_t mem) {
    std::cout << "Found " << ucast << " (" << mcast << ") with free space " << mem << std::endl;
  }

  inline void downloading_failed(const std::string& file, const std::string& addr, in_port_t port, const std::string& desc) {
    std::cout << "File " << file << " downloading failed ("<< addr << ":" << (port == 0 ? "" : std::to_string(port)) << ") " << desc << std::endl;
  }

  inline void uploading_failed(const std::string& file, const std::string& addr, in_port_t port, const std::string& desc) {
    std::cout << "File " << file << " uploading failed ("<< addr << ":" << (port == 0 ? "" : std::to_string(port)) << ") " << desc << std::endl;
  }

  inline void not_exists(const std::string& file) {
    std::cout << "File " << file << " does not exist" << std::endl;
  }

  inline void too_big(const std::string& file) {
    std::cout << "File " << file << " too big" << std::endl;
  }

  inline void searched(const std::string& file, const std::string& addr) {
    std::cout << file << " (" << addr << ")" << std::endl;
  }

  inline void err(const std::string& s) {
    std::cerr << s << " (" << std::strerror(errno) << ")" << std::endl;
  }
}

#endif // _HELPER_HH
