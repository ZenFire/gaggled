// L I C E N S E #############################################################//

/*
 *  Copyright 2011 BigWells Technology (Zen-Fire)
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

// I N C L U D E S ###########################################################//

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>

#include "gv.hpp"
#include "Program.hpp"
#include "Gaggled.hpp"

using namespace gaggled;

Gaggled* g = NULL;

void die_callback(int a) {
  if (g == NULL) {
    // don't try to shutdown if we didn't even create a daemon yet
    exit(0);
  }
  g->stop();
}

void usage() {
  std::cout << "gaggled v" << gaggled::version << ", process manager for running a gaggle of daemons." << std::endl << std::endl;
  std::cout << "usage: gaggled (-h|-c <file> [-t])" << std::endl;
  std::cout << "\t-c <file> where file is the configuration file." << std::endl;
  std::cout << "\t-h to show help." << std::endl;
  std::cout << "\t-t to only test the configuration rather than running it." << std::endl;
}

int main(int argc, char** argv) {
  char* conf_file = NULL;
  bool config_test = false;
  bool help = false;

  int c;
  while ((c = getopt(argc, argv, "htc:")) != -1) {
    switch(c) {
      case 'h':
        help = true;
        break;
      case 't':
        config_test = true;
        break;
      case 'c':
        conf_file = optarg;
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
    if (conf_file or config_test) {
      usage();
      return 1;
    } else {
      usage();
      return 0;
    }
  }

  if (conf_file == NULL) {
    usage();
    return 2;
  }
  
  boost::property_tree::ptree config;

  try {
    boost::property_tree::read_info(conf_file, config);
  } catch (boost::property_tree::info_parser::info_parser_error& pe) {
    std::cout << "configtest: parse error at " << pe.filename() << ":" << pe.line() << ": " << pe.message() << std::endl;
    return 1;
  }

  try {
    g = new Gaggled(config);
  } catch (BadConfigException& bce) {
    std::cout << "configtest: failed, " << bce.reason << std::endl;
    return 1;
  }

  if (config_test) {
    std::cout << "configtest: ok" << std::endl;
    delete g;
    return 0;
  }

  signal(SIGTERM, die_callback);
  signal(SIGINT, die_callback);
  g->run();
  delete g;

  return 0;
}
