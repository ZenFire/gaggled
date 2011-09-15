#include "time.h"
#include "gaggled_events_client.hpp"
#include <iostream>
#include <memory>

class GaggledWatcher : public gaggled_events_client::gaggled_events<GaggledWatcher> {
  public:
  GaggledWatcher(const char* url) :
    gaggled_events_client::gaggled_events<GaggledWatcher>(url)
  {
    
  }
  void handle_statechange(gaggled_events_client::StateChange& obj) {
    if (obj.up == 1) {
      std::cout << "[U] " << obj.program << std::endl;
    } else {
      std::cout << "[D exp=" << int(obj.during_shutdown) << " typ=" << obj.down_type << "] " << obj.program << std::endl;
    }
  }
};

void usage() {
  std::cout << "usage: gaggled_listener (-h|-u <url>)" << std::endl;
  std::cout << "\t-u <url> where url is the ZeroMQ url to connect to." << std::endl;
  std::cout << "\t-h to show help." << std::endl;
}

int main(int argc, char** argv) {
  const char* url = NULL;
  bool help = false;

  int c;
  while ((c = getopt(argc, argv, "hu:")) != -1) {
    switch(c) {
      case 'h':
        help = true;
        break;
      case 'u':
        url = optarg;
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
    if (url) {
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
  
  std::shared_ptr<GaggledWatcher> gw(new GaggledWatcher(url));

  while(1) {
    gw->run_once(50000); // 50ms
  }

  return 0;
}
