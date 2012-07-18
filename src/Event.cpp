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

#include <sys/time.h>
#include <signal.h>
#include <iostream>
#include <ostream>
#include "Gaggled.hpp"
#include "Event.hpp"
#include "Dependency.hpp"
#include "Program.hpp"

// B A S E   E V E N T ######################################################//

gaggled::Event::Event(gaggled::Gaggled* g, gaggled::Dependency* d, gaggled::Program* p, pid_t pid, int delay, int priority) :
  g(g),
  d(d),
  p(p),
  pid(pid),
  priority(priority)
{
  this->set_delay(delay);
  this->queue();
}

void gaggled::Event::set_delay(int delay) {
  this->delay = delay;
  if (delay != 0) {
    // ### get the time of day first
    // http://en.wikipedia.org/wiki/Year_2038_problem
    if (gettimeofday(&(this->when), NULL) != 0)
      std::cout << "error: failed to gettimeofday(), timing behaviour warning." << std::endl;

    // ### convert the delay into sec and usec portions
    // get sec portion
    int delay_sec = delay / 1000;
    // get only usec portion
    int delay_usec = (delay - delay_sec * 1000) * 1000;

    // ### update the when time structure to indicate the timeval when the event is valid to be handled
    // apply seconds
    this->when.tv_sec += delay_sec;
    // cumulative (now + more) usec
    delay_usec = this->when.tv_usec + delay_usec;
    if (delay_usec >= 1000000) {
      // carry
      delay_usec -= 1000000;
      this->when.tv_sec++;
    }
    // new value!
    this->when.tv_usec = delay_usec;
  }
}

bool gaggled::Event::yet() {
  if (this->delay == 0) {
    return true;
  } else {
    timeval tn;
    if (gettimeofday(&tn, NULL) != 0)
      std::cout << "error: failed to gettimeofday(), timing behaviour warning." << std::endl;
    if (tn.tv_sec > this->when.tv_sec) {
      return true;
    } else if ((tn.tv_sec == this->when.tv_sec) && (tn.tv_usec >= this->when.tv_usec)) {
      return true;
    }
  }
  return false;
}

void gaggled::Event::queue() {
  this->g->event_queues[this->priority]->push(this);
}

bool gaggled::Event::handle() {
  std::cout << "handled by doing nothing.\n";
  return true;
}

gaggled::Program* gaggled::Event::get_program_pointer() {
  return p;
}

std::string gaggled::Event::to_string() {
  std::string r = "Base Event";
  return r;
}

std::ostream& gaggled::operator<< (std::ostream &stream, gaggled::Event& e) {
  stream << (&e)->to_string();
  return stream;
}

// S T A R T   E V E N T #############################################//

bool gaggled::StartEvent::handle() {
  if (this->p->is_running())
    return true;

  if (this->p->is_operator_shutdown())
    return true;
  
  if (not this->p->dependencies_satisfied()) {
    this->set_delay(this->g->startwait);
    this->queue();
    return false;
  }

  this->p->start(this->g);
  return true;
}

std::string gaggled::StartEvent::to_string() {
  std::string r = "Start Event";
  return r;
}

gaggled::StartEvent::StartEvent(gaggled::Gaggled* g, gaggled::Program* p) :
  gaggled::Event(g, NULL, p, 0, 0, QPRI_START) {}

// K I L L   E V E N T ###############################################//

bool gaggled::KillEvent::handle() {
  if (this->signal == SIGTERM) {
    new KillEvent(this->g, this->p, SIGKILL, this->prop, this->token, g->killwait);
  }
  this->p->kill_program(this->g, this->signal, this->prop, this->token);
  return true;
}

std::string gaggled::KillEvent::to_string() {
  return std::string("Kill Event");
}

gaggled::KillEvent::KillEvent(gaggled::Gaggled* g, gaggled::Program* p, int signal, bool prop, bool ignore_token) :
  gaggled::Event(g, NULL, p, 0, 0, QPRI_KILL),
  signal(signal),
  prop(prop)
{
  if (ignore_token) {
    this->token = 0;
  } else {
    this->token = p->get_token();
  }
}

gaggled::KillEvent::KillEvent(gaggled::Gaggled* g, gaggled::Program* p, int signal, bool prop, unsigned long long token, int delay) :
  gaggled::Event(g, NULL, p, 0, delay, QPRI_KILL),
  signal(signal),
  prop(prop),
  token(token)
{}
  

// D I E D   E V E N T ###############################################//

bool gaggled::DiedEvent::handle() {
  if (this->g->pid_map.find(this->pid) == this->g->pid_map.end()) {
    std::cout << "unknown child " << this->pid << " died. discarding.\n";
  } else {
    this->g->pid_map[this->pid]->died(this->g, down_type, rcode);
  }
  return true;
}

std::string gaggled::DiedEvent::to_string() {
  return std::string("Died Event");
}

gaggled::DiedEvent::DiedEvent(gaggled::Gaggled* g, pid_t pid, std::string down_type, int rcode) :
  gaggled::Event(g, NULL, NULL, pid, 0, QPRI_DIED),
  rcode(rcode),
  down_type(down_type)
{}
