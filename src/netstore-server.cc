#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <filesystem>

class server {
 public:
  server(std::string ma, uint16_t cp, int64_t ms, std::string sf, uint8_t t) :
    MCAST_ADDR(std::move(ma)),
    CMD_PORT(cp),
    MAX_SPACE(ms),
    SHRD_FLDR(std::move(sf)),
    TIMEOUT(t)
  {}

  void connect();

 private:
  std::string MCAST_ADDR;
  uint16_t CMD_PORT;
  int64_t MAX_SPACE;
  std::string SHRD_FLDR;
  uint8_t TIMEOUT;
};

void server::connect() {

}

template<typename T>
void check_range(const T& value, const T& min, const T& max, const std::string& param) {
  namespace bpo = boost::program_options;
  if (value < min || value > max) {
    throw bpo::validation_error(
        bpo::validation_error::invalid_option_value, param, std::to_string(value)
    );
  }
}

int main(int ac, char** av) {
  namespace bpo = boost::program_options;
  std::string MCAST_ADDR;
  int64_t CMD_PORT;
  int64_t MAX_SPACE;
  std::string SHRD_FLDR;
  int64_t TIMEOUT;

  bpo::options_description desc("Allowed options");
  desc.add_options()
      (",g",
       bpo::value(&MCAST_ADDR)
         ->required(),
       "Multicast address")
      (",p",
       bpo::value(&CMD_PORT)
         ->required()
         ->notifier(
           boost::bind(&check_range<int64_t>, _1, 0, std::numeric_limits<uint16_t>::max(), "p")
         ),
       "UDP port")
      (",b",
       bpo::value(&MAX_SPACE)
         ->default_value(52428800)
         ->notifier(
           boost::bind(&check_range<int64_t>, _1, 0, std::numeric_limits<int64_t>::max(), "b")
         ),
       "Allowed space")
      (",f",
       bpo::value(&SHRD_FLDR)
         ->required()
         ->notifier(
          [] (const std::string& s) {
            if (not std::filesystem::is_directory(s))
              throw bpo::validation_error(
                  bpo::validation_error::invalid_option_value, "f", s
              );
         }),
       "Shared folder")
      (",t",
       bpo::value(&TIMEOUT)
         ->default_value(5)
         ->notifier(
           boost::bind(&check_range<int64_t>, _1, 0, 300, "t")
         ),
       "Timeout")
      ;

  try {
    bpo::variables_map vm;
    store(bpo::parse_command_line(ac, av, desc), vm);
    notify(vm);

    server s(MCAST_ADDR, CMD_PORT, MAX_SPACE, SHRD_FLDR, TIMEOUT);
    s.connect();
    std::cout << MCAST_ADDR << " " << CMD_PORT << std::endl;
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
  }
}
