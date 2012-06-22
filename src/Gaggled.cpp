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

#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <iostream>
#include <string>
#include <queue>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "Event.hpp"
#include "Gaggled.hpp"
#include "gaggled_control_server.hpp"
#include "gaggled_events_server.hpp"

gaggled::Gaggled::Gaggled(char* conf_file) :
  stopped(false),
  tick(10),
  startwait(100),
  killwait(10000)
{
  for (int i = 0; i != QPRI_END; i++)
    this->event_queues[i] = new std::queue<Event*>();
  try {
    this->parse_config(conf_file);
  } catch (...) {
    clean_up();
    throw;
  }
}

gaggled::Gaggled::~Gaggled() {
  clean_up();
}

void gaggled::Gaggled::clean_up() {
  if (path != NULL) {
    free(path);
    path = NULL;
  }
  for (auto d = dependencies.begin(); d != dependencies.end(); d++)
    delete *d;
  for (auto p = programs.begin(); p != programs.end(); p++)
    delete *p;
  for (int i = 0; i != QPRI_END; i++) {
    if (event_queues[i] == NULL)
      continue;
    while (not event_queues[i]->empty()) {
      delete event_queues[i]->front();
      event_queues[i]->pop();
    }
    delete event_queues[i];
    event_queues[i] = NULL;
  }
}

// TODO undo duplication here
void gaggled::Gaggled::write_state(gaggled_events_server::ProgramState& sc, Program* p) {
  sc.program = p->getName();
  sc.up = (p->is_running() ? 1 : 0);
  sc.dependencies_satisfied = (p->dependencies_satisfied() ? 1 : 0);
  sc.is_operator_shutdown = (p->is_operator_shutdown() ? 1 : 0);
  sc.state_sequence = p->state_changes() + 1;
  if (sc.up) {
    sc.during_shutdown = 0;
    sc.down_type = "NONE";
    sc.pid = p->get_pid();
  } else {
    sc.during_shutdown = (p->is_controlled_shutdown() ? 1 : 0);
    sc.down_type = p->getDownType();
    sc.pid = 0;
    sc.uptime_ms = 0;
  }
}
void gaggled::Gaggled::write_state(gaggled_control_server::ProgramState& sc, Program* p) {
  sc.program = p->getName();
  sc.up = (p->is_running() ? 1 : 0);
  sc.dependencies_satisfied = (p->dependencies_satisfied() ? 1 : 0);
  sc.is_operator_shutdown = (p->is_operator_shutdown() ? 1 : 0);
  sc.state_sequence = p->state_changes() + 1;
  if (sc.up) {
    sc.during_shutdown = 0;
    sc.down_type = "NONE";
    sc.pid = p->get_pid();
    sc.uptime_ms = p->uptime();
  } else {
    sc.during_shutdown = (p->is_controlled_shutdown() ? 1 : 0);
    sc.down_type = p->getDownType();
    sc.pid = 0;
    sc.uptime_ms = 0;
  }
}

void gaggled::Gaggled::broadcast_state(Program* p) {
  if (eventserver != NULL) {
    gaggled_events_server::ProgramState sc;
    write_state(sc, p);
    eventserver->pub_statechange(sc);
  }
}

void gaggled::Gaggled::read_env_config(boost::property_tree::ptree& pt, std::map<std::string, std::string>* write_to) {
    for (auto e = pt.begin(); e != pt.end(); e++) {
      std::string e_name = e->first;
      std::string e_val = pt.get<std::string>(e_name);
      (*write_to)[e_name] = e_val;
    }
}

#define SMTP_CONFIGTEST_FAIL(msg) throw gaggled::BadConfigException(msg);

void gaggled::Gaggled::parse_config(char* conf_file) {
  boost::property_tree::ptree pt;
  boost::property_tree::read_info(conf_file, pt);

  // get $PATH
  this->path = getenv("PATH");
  if (this->path != NULL)
    this->path = strdup(this->path);

  // convert environ into a map<string,string>
  std::map<std::string, std::string> env_map;
  for (int i = 0; environ[i] != NULL; i++) {
    char* env_entry = strdup(environ[i]);
    if (env_entry == NULL)
      continue;

    char* env_value = strstr(env_entry, "=");
    if (env_value != NULL) {
      // can't use strtok_r because there can be ='s in the var value itself
      env_value[0] = 0;
      env_value++;
      env_map[env_entry] = env_value;
    } else {
      std::cout << "warning: env entry missing =, skipping" << std::endl;
    }
    free(env_entry);
  }

  for (auto iter = pt.begin(); iter != pt.end(); iter++) {
    std::string name = iter->first;

    if (name == "gaggled") {
      this->tick = iter->second.get<int>("tick", this->tick);
      this->startwait = iter->second.get<int>("startwait", this->startwait);
      this->killwait = iter->second.get<int>("killwait", this->killwait);
      this->eventurl = iter->second.get<std::string>("eventurl", this->eventurl);
      this->controlurl = iter->second.get<std::string>("controlurl", this->controlurl);

      std::string pre_path = iter->second.get<std::string>("path", "");
      if (pre_path != "") {
        if (path != NULL) {
          char* p = strdup((pre_path + ":" + path).c_str());
          free(path);
          path = p;
        } else {
          path = strdup(pre_path.c_str());
        }

        setenv("PATH", path, 1);
        env_map["PATH"] = std::string(path);
      }

      if (iter->second.get<bool>("smtpgate.auto", false)) {
        {
          #include "gaggled_smptgate_config.hpp"
        }

        std::vector<std::string>* smtpgate_args = new std::vector<std::string>();
        smtpgate_args->push_back("-c");
        smtpgate_args->push_back(std::string(conf_file));
        std::map<std::string, std::string> own_env;
        Program* smtpgate = new Program("smtpgate", "gaggled_smtpgate", smtpgate_args, own_env, "", true, true);
        this->programs.push_back(smtpgate);
        this->program_map["smtpgate"] = smtpgate;
      }
    } else {
      // first check for dupe
      if (this->program_map.find(name) != this->program_map.end()) {
        throw gaggled::BadConfigException(std::string("duplicate program name ") + name);
      }

      std::string command = iter->second.get<std::string>("command", "");
      if (command == "")
        throw gaggled::BadConfigException("program " + name + " missing setting: command.");

      std::string argv = iter->second.get<std::string>("argv", "");
      std::vector<std::string>* argv_vec = new std::vector<std::string>();

      if (argv != "")
        boost::split(*argv_vec, argv, boost::is_any_of(" \t"), boost::token_compress_on);

      std::string wd = iter->second.get<std::string>("wd", "");

      bool respawn = iter->second.get<bool>("respawn", true);
      bool enabled = iter->second.get<bool>("enabled", true);

      // read program specific env
      std::map<std::string, std::string> own_env;
      boost::optional<boost::property_tree::ptree&> env_o = iter->second.get_child_optional("env");
      if (env_o) {
        read_env_config(*env_o, &own_env);
      }

      Program* p = new Program(name, command, argv_vec, own_env, wd, respawn, enabled);
      this->programs.push_back(p);
      this->program_map[name] = p;

      boost::optional<boost::property_tree::ptree&> depends = iter->second.get_child_optional("depends");
      if (depends) {
        for (auto dep = depends->begin(); dep != depends->end(); dep++) {
          std::string on = dep->first;
          int delay = dep->second.get<int>("delay", 0);
          bool propagate = dep->second.get<bool>("propagate", false);
          
          Dependency* d = new Dependency(name, on, delay, propagate);
          this->dependencies.push_back(d);
        }
      }
    }
  }

  if (this->path != NULL) {
    char *path_ptr, *path_split;
    path_ptr = path_split = strdup(this->path);
    char *s, *d;
    while ((d = strtok_r(path_ptr, ":", &s)) != NULL) {
      path_ptr = NULL;
      paths.push_back(std::string(d));
    }

    free(path_split);
  }

  // overlay environments and do $PATH searches
  for (auto p = this->programs.begin(); p != this->programs.end(); p++) {
    (*p)->overlay_environment(env_map);
    if (not (*p)->search(&(this->paths)))
      throw gaggled::BadConfigException("program " + (*p)->get_command() + " not found, not a file, or not executable");
  }

  // link up the graph now that we have all programs loaded
  for (auto d = this->dependencies.begin(); d != this->dependencies.end(); d++) {
    (*d)->link(this);
  }
}

gaggled::GaggledController::GaggledController(Gaggled* g, const char* url) :
  gaggled_control_server::gaggled_control<GaggledController>(url),
  g(g)
{}

uint8_t gaggled::GaggledController::handle_start (std::string req) {
  try {
    Program* p = g->get_program(req);
    std::cout << "[ctrl] starting " << req << std::endl;
    p->op_start(g);
    return 0;
  } catch (BadConfigException& bce) {
    return 1;
  }
}
uint8_t gaggled::GaggledController::handle_kill (std::string req) {
  try {
    Program* p = g->get_program(req);
    std::cout << "[ctrl] killing " << req << std::endl;
    p->op_kill(g);
    return 0;
  } catch (BadConfigException& bce) {
    return 1;
  }
}
uint8_t gaggled::GaggledController::handle_stop (std::string req) {
  try {
    Program* p = g->get_program(req);
    std::cout << "[ctrl] stopping " << req << std::endl;
    p->op_shutdown(g);
    return 0;
  } catch (BadConfigException& bce) {
    return 1;
  }
}

std::vector<gaggled_control_server::ProgramState> gaggled::GaggledController::handle_getstates (int32_t req) {
  // too noisy to be there in production builds std::cout << "[ctrl] getstates" << std::endl;

  std::vector<gaggled_control_server::ProgramState> states;

  for (auto p = g->programs.begin(); p != g->programs.end(); p++) {
    gaggled_control_server::ProgramState ps;
    g->write_state(ps, *p);
    states.push_back(ps);
  }
  
  if (g->eventserver != NULL)
    g->eventserver->pub_dumped(req);

  return states;
}

void gaggled::Gaggled::run() {
  eventserver = NULL;
  controlserver = NULL;

  if (controlurl != "") {
    controlserver = new GaggledController(this, controlurl.c_str());
  }
  if (eventurl != "") {
    eventserver = new gaggled_events_server::gaggled_events(eventurl.c_str());
  }

  // kick off start of enabled processes
  for (auto p = this->programs.begin(); p != this->programs.end(); p++)
    if (!(*p)->is_operator_shutdown())
      new gaggled::StartEvent(this, *p);

  bool known_stopped = false;
  std::cout << "[gaggled] running. tick=" << this->tick << ", controlurl=" << this->controlurl << ", eventurl=" << this->eventurl << ", PATH=" << this->path << std::endl << std::flush;
  while ((not this->stopped) or (this->pid_map.begin() != this->pid_map.end())) {
    // check if this is the first event loop run that is in the shutdown mode
    // we have to kick off the creation of the kill events, as we couldn't do that in the signal handler
    // that called stop() - this could lock.
    if (known_stopped != this->stopped) {
      std::cout << "[gaggled] caught signal, shutting down." << std::endl << std::flush;
      for (auto p = this->programs.begin(); p != this->programs.end(); p++)
        new KillEvent(this, *p, SIGTERM, false, true);
    }

    // and now we know..
    known_stopped = this->stopped;

    // first, check if any child processes have died.
    this->check_deaths();

    // don't loop forever in each loop. We need to get back to the other queue, or starvation could result.
    // if currently processed keep creating new events in the current queue, this will result in issues.
    // so instead of using the queues directly, we move all presently queued objects into the now_queue and use that queue
    std::queue<gaggled::Event*> now_queue;    
    gaggled::Event* e;
    int processed = 0;

    // during shutdown, we will only process events with priority <QPRI_THRESH_SHUTDOWN
    int endpri;
    if (this->stopped) {
      endpri = QPRI_THRESH_SHUTDOWN;
    } else {
      endpri = QPRI_END;
    }

    // in priority order, empty queues and process each event
    for (int priority = QPRI_BEGIN; priority != endpri; priority++) {
      std::queue<gaggled::Event*>* q = this->event_queues[priority];
      while (not q->empty()) {
        now_queue.push(q->front());
        q->pop();
      }

      while (not now_queue.empty()) {
        e = now_queue.front();
        now_queue.pop();

        if (not e->yet()) {
          // re-queue events that are not yet runnable
          e->queue();
        } else {
          // if an event returns true, it is done with and should be deleted
          if (e->handle()) {
            // e has dealt with itself, but cannot delete itself. delete!
            delete e;
            processed++;
          }
        }
      }
    }

    // events are processed, now nap a little if we did nothing this time;
    // otherwise, get right back into it!
    if (controlserver == NULL) {
      if (processed == 0) {
        usleep(1000 * this->tick);
      }
    } else {
      try {
        if (processed == 0) {
          controlserver->run_once(1000 * this->tick);
        } else {
          controlserver->run_once(0);
        }
      } catch (gaggled_control_server::BadMessage& gcs_bm) {
        std::cout << "[gaggled] got an bad incoming message on control channel, discarding." << std::endl << std::flush;
      }
    }
  }

  if (eventserver != NULL)
    delete eventserver;
  if (controlserver != NULL)
    delete controlserver;
}

void gaggled::Gaggled::check_deaths() {
  int callstatus = 0;
  siginfo_t siginfo;

  while (callstatus == 0) {
    siginfo.si_pid = 0;
    callstatus = waitid(P_ALL, 0, &siginfo, WNOHANG|WEXITED);
    if (callstatus == 0) {
      if (siginfo.si_pid == 0) {
        // if there are children and none of them changed state, we get 0 return but si_pid is unchanged
        return;
      }

      bool exited = false;
      int rcode = 0;
      std::string down_type = "UNK";

      if (siginfo.si_code == CLD_EXITED) {
        exited = true;
        rcode = siginfo.si_status;
        down_type = "EXIT";
      } else if (siginfo.si_code == CLD_DUMPED) {
        exited = false;
        down_type = "DUMP";
      } else if (siginfo.si_code == CLD_KILLED) {
        exited = false;
        down_type = "KILL";
      }

      std::cout << "[gaggled] child pid=" << siginfo.si_pid << " died. exited:" << exited << " status:" << rcode << std::endl;
      new gaggled::DiedEvent(this, siginfo.si_pid, down_type, rcode);
    }
  }
}

void gaggled::Gaggled::stop() {
  this->stopped = true;
}

bool gaggled::Gaggled::is_running() {
  return not this->stopped;
}

gaggled::Program* gaggled::Gaggled::get_program(std::string name) {
  if (program_map.find(name) == program_map.end())
    throw gaggled::BadConfigException("program " + name + " does not exist.");
  return program_map[name];
}
