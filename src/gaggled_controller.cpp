#include "gaggled_control_client.hpp"
#include <iostream>
#include <string>
#include <memory>
#include <boost/lexical_cast.hpp>
#include "gv.hpp"

void usage() {
  std::cout << "gaggled_controller v" << gaggled::version << ", tool for controlling a running gaggled instance." << std::endl << std::endl;
  std::cout << "usage: gaggled_controller (-h|-u <url> (-s <program>|-k <program>|-d))" << std::endl;
  std::cout << "\t-u <url> where url is the ZeroMQ url to connect to." << std::endl;
  std::cout << "\t-s <program> take program out of admin down state and direct gaggled to attempt to start it." << std::endl;
  std::cout << "\t-r <program> direct gaggled to shut it down the program; if configured to restart, it will restart." << std::endl;
  std::cout << "\t-k <program> put program in admin down state and direct gaggled to shut it down." << std::endl;
  std::cout << "\t-d to dump a summary of the programs running and their states." << std::endl;
  std::cout << "\t-h to show help." << std::endl;
}

const int ACT_NONE = 0;
const int ACT_KILL = 1;
const int ACT_RESTART = 2;
const int ACT_START = 3;
const int ACT_DUMP = 4;

int main(int argc, char** argv) {
  bool help = false;

  const char* url = NULL;
  const char* program = NULL;
  int action = ACT_NONE;

  int gs;
  while ((gs = getopt(argc, argv, "hds:k:r:u:")) != -1) {
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
      case 'r':
        if (action != ACT_NONE)
          {
          usage();
          return 1;
          }
        action = ACT_RESTART;
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
  } else if (action == ACT_RESTART) {
    if (c->call_kill(program) != 0) {
      std::cerr << "unknown program " << program << std::endl;
      std::exit(1);
    }
  } else if (action == ACT_KILL) {
    if (c->call_stop(program) != 0) {
      std::cerr << "unknown program " << program << std::endl;
      std::exit(1);
    }
  } else if (action == ACT_DUMP) {
    std::vector<gaggled_control_client::ProgramState> ps = c->call_getstates(0);

    int up = 0;
    int opdown = 0;
    int down = 0;

    for (auto p = ps.begin(); p != ps.end(); p++) {
      if (p->is_operator_shutdown == 1 && p->up == 0) 
        opdown++;
      else if (p->up == 1)
        up++;
      else
        down++;
    }

    std::cout << "up: " << up << " down: " << down << " opdown: " << opdown << std::endl;

    size_t maxname = 0;
    size_t maxpid = 0;
    for (auto p = ps.begin(); p != ps.end(); p++) {
      size_t nl = p->program.length();
      if (nl > maxname)
        maxname = nl;
      size_t np = boost::lexical_cast<std::string>(p->pid).length();
      if (np > maxpid)
        maxpid = np;
    }

    for (auto p = ps.begin(); p != ps.end(); p++) {
      if (p->is_operator_shutdown == 1 && p->up == 0)
        std::cout << "[OPDOWN";
      else if (p->up == 1)
        std::cout << "[UP    ";
      else
        std::cout << "[DOWN  ";

      std::string pid_s = boost::lexical_cast<std::string>(p->pid);

      std::cout << "] " << p->program << std::string(maxname + 1 - p->program.length(), ' ');
      std::cout << p->pid << std::string(maxpid + 1 - pid_s.length(), ' ');
      
      if (p->up) {
        std::string ms = boost::lexical_cast<std::string>(p->uptime_ms % 1000);
        
        std::cout << "up " << (p->uptime_ms / 1000) << "." << std::string(3 - ms.length(), '0') << ms << "s";
      }

      std::cout << std::endl;
    }
  }
  
  return 0;
}
