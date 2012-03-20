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
#include <ostream>
#include <boost/lexical_cast.hpp>
#include "Dependency.hpp"
#include "Gaggled.hpp"
#include <set>
#include <queue>

gaggled::Dependency::Dependency(gaggled::Program* of, gaggled::Program* on, int delay, bool propagate) :
  of(of),
  on(on),
  delay(delay),
  propagate(propagate)
{}

gaggled::Dependency::Dependency(std::string of, std::string on, int delay, bool propagate) :
  of(NULL),
  on(NULL),
  of_name(of),
  on_name(on),
  delay(delay),
  propagate(propagate)
{}

std::string gaggled::Dependency::to_string() {
  std::string r = "Dependency of " + of_name + " on " + on_name + " delay:" + boost::lexical_cast<std::string>(this->delay) + " propagate:" + boost::lexical_cast<std::string>(this->propagate);
  return r;
}

bool gaggled::Dependency::is_of(Program* p) {
  return this->of == p;
}

bool gaggled::Dependency::is_on(Program* p) {
  return this->on == p;
}

void gaggled::Dependency::link(Gaggled* g) {
  try {
    of = g->get_program(of_name);
  } catch (BadConfigException& bce) {
    throw gaggled::BadConfigException("dependency of linkage failed: " + bce.reason);
  }
  try {
    on = g->get_program(on_name);
    // TODO update docs about depending on a disabled program, we sort of have to make that allowed now.
    /*
    if (of->is_enabled() and not on->is_enabled())
      throw gaggled::BadConfigException("cannot depend on disabled program " + on->getName());
    */
  } catch (BadConfigException& bce) {
    throw gaggled::BadConfigException("dependency on linkage failed: " + bce.reason);
  }

  // check now via BFS if we can reach 'of' by following outbound dependencies from 'on'; this means that we'll be creating
  // a dependency cycle which is not tolerable.
  {
    std::set<Program*> cs; // cycle set; the set that we should not find dependencies on
    std::queue<Program*> q; // queue; the set of programs to check outbound dependencies from

    cs.insert(of);
    q.push(on);

    while (not q.empty()) {
      Program* n = q.front();
      q.pop();

      if (cs.find(n) != cs.end()) {
        throw gaggled::BadConfigException(of->getName() + " cannot depend on " + on->getName() + " as this would create a dependency cycle.");
      }

      cs.insert(n);

      auto outbound = n->get_dependencies();
      for (auto o = outbound.begin(); o != outbound.end(); o++) {
        q.push((*o)->on);
      }
    }
  }

  on->add_dependency(this);
  of->add_dependency(this);
}

bool gaggled::Dependency::satisfied() {
  // ask the program we depend on if it's been up long enough.
  return this->on->is_up(this->delay);
}

void gaggled::Dependency::prop_down(Gaggled* g) {
  if (this->propagate) {
    // this dependency has propagation turned on, so we should create a conditional restarting kill event.
    if (g->is_running())
      new KillEvent(g, this->of, SIGTERM, true, false);
  }
}

std::ostream& gaggled::operator<< (std::ostream &stream, gaggled::Dependency& d) {
  stream << (&d)->to_string();
  return stream;
}
