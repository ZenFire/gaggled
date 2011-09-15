#ifndef GAGGLED_DEPENDENCY_HPP_INCLUDED
#define GAGGLED_DEPENDENCY_HPP_INCLUDED

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

#include "Program.hpp"

namespace gaggled {
class Program;
class Gaggled;
class Dependency
{
public:
  Dependency(std::string of, std::string on, int delay, bool propagate);
  Dependency(Program* of, Program* on, int delay, bool propagate);
  std::string to_string();
  bool is_of(Program* p);
  bool is_on(Program* p);
  void link(Gaggled* g);
  bool satisfied();
  void prop_down(Gaggled* g);
private:
  Program* of;
  Program* on;
  std::string of_name;
  std::string on_name;
  int delay;
  bool propagate;
};
std::ostream &operator<< (std::ostream &stream, Dependency& d);
}

#endif
