#include "gaggled_control_client.hpp"
#include <iostream>
#include <string>
#include <memory>

void usage() {
  std::cout << "usage: gaggled_controller (-h|-u <url> (-s <program>|-k <program>|-d))" << std::endl;
  std::cout << "\t-u <url> where url is the ZeroMQ url to connect to." << std::endl;
  std::cout << "\t-s <program> take program out of admin down state and direct gaggled to attempt to start it." << std::endl;
  std::cout << "\t-k <program> put program in admin down state and direct gaggled to shut it down." << std::endl;
  std::cout << "\t-d to dump a summary of the programs running and their states." << std::endl;
  std::cout << "\t-h to show help." << std::endl;
}

const int ACT_NONE = 0;
const int ACT_KILL = 1;
const int ACT_START = 2;
const int ACT_DUMP = 3;

int main(int argc, char** argv) {
  bool help = false;

  const char* url = NULL;
  const char* program = NULL;
  int action = ACT_NONE;

  int gs;
  while ((gs = getopt(argc, argv, "hds:k:u:")) != -1) {
    switch(gs) {
      case 'h':
        help = true;
        break;
      case 'u':
        url = optarg;
        break;
      case 'd':
        if (action != ACT_NONE)
          {
          usage();
          return 1;
          }
        action = ACT_DUMP;
        break;
      case 's':
        if (action != ACT_NONE)
          {
          usage();
          return 1;
          }
        action = ACT_START;
        program = optarg;
        break;
      case 'k':
        if (action != ACT_NONE)
          {
          usage();
          return 1;
          }
        action = ACT_KILL;
        program = optarg;
        break;
      case '?':
        usage();
        return 1;
      default :
        usage();
        return 1;
    }
  }

  if (help) {
    if (url != NULL || program != NULL || action != ACT_NONE) {
      usage();
      return 1;
    } else {
      usage();
      return 0;
    }
  }

  if (url == NULL) {
    usage();
    return 2;
  }

  if (action == ACT_NONE) {
    usage();
    return 2;
  }

  std::shared_ptr<gaggled_control_client::gaggled_control> c(new gaggled_control_client::gaggled_control(url));

  if (argc < 2) {
    usage();
  }

  if (action == ACT_START) {
    if (c->call_start(program) != 0) {
      std::cerr << "unknown program " << program << std::endl;
      std::exit(1);
    }
  } else if (action == ACT_KILL) {
    if (c->call_stop(program) != 0) {
      std::cerr << "unknown program " << program << std::endl;
      std::exit(1);
    }
  } else if (action == ACT_DUMP) {
    gaggled_control_client::ProgramStates ps = c->call_getstate(0);
    std::cout << "up: " << ps.up.size() << " down: " << ps.down.size() << std::endl;
    for (auto s = ps.up.begin(); s != ps.up.end(); s++)
      std::cout << "[U] " << *s << std::endl;
    for (auto s = ps.down.begin(); s != ps.down.end(); s++)
      std::cout << "[D] " << *s << std::endl;
  }
  
  return 0;
}
