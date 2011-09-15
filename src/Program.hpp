#ifndef GAGGLED_PROGRAM_HPP_INCLUDED
#define GAGGLED_PROGRAM_HPP_INCLUDED

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

#include <ostream>
#include <string>
#include <map>
#include <vector>
#include "Gaggled.hpp"

namespace gaggled {
class Dependency;
class Program
{
public:
  Program(std::string name, std::string command, std::vector<std::string>* argv, std::map<std::string, std::string> own_env, bool respawn, bool enabled);
  ~Program();
  void overlay_environment(std::map<std::string, std::string> global_environment);
  std::string to_string();
  std::string getName();
  bool search(std::vector<std::string>* path);
  void add_dependency(Dependency* d);
  bool is_enabled();
  bool is_running();
  std::string get_command();
  bool dependencies_satisfied();
  void start(Gaggled* g);
  void kill_program(Gaggled* g, int signal, bool prop_start, unsigned long long token);
  void died(Gaggled* g, std::string down_type, int rcode);
  bool is_up(int ms);
  unsigned long long get_token();
  void op_start(Gaggled* g);
  void op_shutdown(Gaggled* g);
  bool is_operator_shutdown();
  bool is_controlled_shutdown();
  std::vector<Dependency*> get_dependencies();
private:
  //global statics
  static unsigned long long instance_token;
  // run-length settings
  std::string name;
  std::string command;
  std::vector<std::string> commands;
  std::vector<std::string>* argv;
  std::map<std::string, std::string> own_env;
  std::vector<Dependency*>* dependencies;
  bool respawn;
  bool enabled;
  bool operator_shutdown;
  bool controlled_shutdown;
  char **exec_argv;
  char **exec_env;
  // changable state
  bool running;
  bool prop_start;
  pid_t pid;
  timeval started;
  unsigned long long token;
};
std::ostream &operator<< (std::ostream &stream, Program& p);
}

#endif
