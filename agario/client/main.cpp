#include <iostream>

#include <dep/cxxopts.hpp>
#include "agario/client/client.hpp"

cxxopts::Options options() {

  try {
    cxxopts::Options options("Agar.io Client", "command line options");
    options
      .positional_help("[optional args]")
      .show_positional_help();

    options.add_options()
      ("s,singleplayer", "singleplayer mode", cxxopts::value<bool>()->default_value("false"))
      ("server", "Server", cxxopts::value<std::string>()->default_value("localhost"))
      ("port", "Port", cxxopts::value<int>()->default_value("80"))
      ("name", "Player Name", cxxopts::value<std::string>()->default_value("unnamed"))
      ("help", "Print help");


    options.parse_positional({"server"});
    return options;

  } catch (const std::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}


int main(int argc, char *argv[]) {
  auto opts = options();
  auto args = opts.parse(argc, argv);

//  bool singleplayer = args["singleplayer"].as<bool>();
  std::string name = args["name"].as<std::string>();


//  if (singleplayer) {
  std::cout << "Single-player mode." << std::endl;

  agario::Client client;
  agario::pid pid = client.add_player(name);

  client.set_player(pid);
  client.add_bots();

  client.play();

//  } else {
//    std::string server = args["server"].as<std::string>();
//    int port = args["port"].as<int>();
//
//    std::cout << "Server: " << server << std::endl;
//    std::cout << "Port: " << port << std::endl;
//
//    agario::Client client(server, port);
//    client.connect();
//    client.game_loop();
//  }

  return 0;
}
