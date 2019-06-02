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

  bool extract_filename(const std::string& word, const std::string& s, std::string& param) {
    std::smatch match;
    auto is_match = std::regex_match(s, std::regex("^" + word + "( (.+)){1}"));

    if (is_match)
      param = s.substr(std::min<size_t>(word.length() + 1, s.length()));

    return is_match;
  }

  bool is_fetch(const std::string& s, std::string& param) {
    return extract_filename("fetch", s, param);
  }

  bool is_upload(const std::string& s, std::string& param) {
    return extract_filename("upload", s, param);
  }

  bool is_remove(const std::string& s, std::string& param) {
    return extract_filename("remove", s, param);
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