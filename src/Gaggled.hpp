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

#include <unistd.h>
#include <vector>
#include <map>
#include <queue>
#include <stdexcept>

#include "Program.hpp"
#include "Dependency.hpp"
#include "Event.hpp"

#include <boost/thread/thread.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>

#include "gaggled_control_server.hpp"
#include "gaggled_events_server.hpp"

#define QPRI_BEGIN 0

#define QPRI_DIED 0
#define QPRI_KILL 1
#define QPRI_START 2

#define QPRI_THRESH_SHUTDOWN 2
#define QPRI_END 3

#ifndef GAGGLED_HPP_INCLUDED
#define GAGGLED_HPP_INCLUDED

namespace gaggled {
// exception for reporting inconsistent or invalid config
class BadConfigException {
public:
  BadConfigException(std::string reason) :
    reason(reason)
  {
  }
  std::string reason;
};
class Event;
class Program;
class Dependency;
class GaggledController;
class Gaggled
{
  friend class Program;
  friend class Event;
  friend class StartEvent;
  friend class DiedEvent;
  friend class KillEvent;
  friend class GaggledController;
public:
  Gaggled(boost::property_tree::ptree pt);
  ~Gaggled();
  void run();
  void check_deaths();
  void stop();
  bool is_running();
  Program* get_program(std::string name);
private:
  gaggled_events_server::gaggled_events* eventserver;
  GaggledController* controlserver;
  bool stopped;
  char* path;
  std::vector<std::string> paths;
  int tick;
  int startwait;
  int killwait;
  std::string eventurl;
  std::string controlurl;
  std::map<std::string, Program*> program_map;
  std::map<pid_t, Program*> pid_map;
  std::vector<Program*> programs;
  std::vector<Dependency*> dependencies;
  std::queue<Event*>* event_queues[QPRI_END];
  void broadcast_state(Program* p, bool up, bool during_shutdown=false, const std::string down_type="UNK");
  void read_env_config(boost::property_tree::ptree& pt, std::map<std::string, std::string>* write_to);
  void parse_config(boost::property_tree::ptree pt);
  void clean_up();
};

class GaggledController : public gaggled_control_server::gaggled_control<GaggledController> {
public:
  GaggledController(Gaggled* g, const char* url);
  uint8_t handle_start (std::string req);
  uint8_t handle_stop (std::string req);
  gaggled_control_server::ProgramStates handle_getstate (uint8_t req);
private:
  Gaggled* g;
};

}

#endif
