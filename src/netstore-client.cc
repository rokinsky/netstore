#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>

int main(int ac, char** av) {
  namespace bpo = boost::program_options;

  bpo::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ;

  return 0;
}
