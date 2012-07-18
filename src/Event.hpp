#ifndef GAGGLED_EVENT_HPP_INCLUDED
#define GAGGLED_EVENT_HPP_INCLUDED

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
#include <time.h>
#include <ostream>
#include "Program.hpp"
#include "Dependency.hpp"
#include "Gaggled.hpp"

namespace gaggled {
class Gaggled;
class Dependency;
class Program;
class Event
{
public:
  Event(Gaggled* g, Dependency* d, Program* p, pid_t pid, int delay, int priority);
  bool yet();
  void queue();
  virtual bool handle();
  virtual std::string to_string();
  virtual Program* get_program_pointer();
protected:
  Gaggled* g;
  Dependency* d;
  Program* p;
  pid_t pid;
  void set_delay(int delay);
private:
  int delay;
  struct timeval when;
  int priority;
};

class StartEvent : public Event {
public: 
  StartEvent(Gaggled* g, Program* p);
  virtual bool handle();
  virtual std::string to_string();
};

class KillEvent : public Event {
public: 
  KillEvent(Gaggled* g, Program* p, int signal, bool prop, bool ignore_token);
  KillEvent(Gaggled* g, Program* p, int signal, bool prop, unsigned long long token, int delay);
  virtual bool handle();
  virtual std::string to_string();
private:
  int signal;
  bool prop;
  unsigned long long token;
};

class DiedEvent : public Event {
public: 
  DiedEvent(Gaggled* g, pid_t pid, std::string down_type, int rcode=0);
  virtual bool handle();
  virtual std::string to_string();
private:
  int rcode;
  std::string down_type;
};

std::ostream &operator<< (std::ostream &stream, Event& p);
}

#endif
