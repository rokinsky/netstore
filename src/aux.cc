#include <string>
#include <regex>
#include <boost/program_options.hpp>
#include <filesystem>

#include "aux.hh"

namespace netstore::aux {
  bool is_discover(const std::string& s) {
    return s == "discover";
  }

  bool is_search(const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^search( |( (.*))?)"));

    if (is_match)
      param = s.substr(std::min<size_t>(strlen("search "), s.length()));

    return is_match;
  }

  bool is_fetch(const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^fetch( (.+)){1}"));

    if (is_match)
      param = s.substr(std::min<size_t>(strlen("fetch "), s.length()));

    return is_match;
  }

  bool is_upload(const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^upload( (.+)){1}"));

    if (is_match)
      param = s.substr(std::min<size_t>(strlen("upload "), s.length()));

    return is_match;
  }

  bool is_remove(const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^remove( (.+)){1}"));

    if (is_match)
      param = s.substr(std::min<size_t>(strlen("remove "), s.length()));

    return is_match;
  }

  bool is_exit(const std::string& s) {
    return s == "exit";
  }

  void check_dir(const std::string& s) {
    namespace bpo = boost::program_options;
    if (!std::filesystem::is_directory(s))
      throw bpo::validation_error(
          bpo::validation_error::invalid_option_value, "f", s
      );
  }
}