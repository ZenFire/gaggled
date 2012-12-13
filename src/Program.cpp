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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sysexits.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>
#include "Program.hpp"
#include "Dependency.hpp"

#define PTOK_INVAL 1
unsigned long long gaggled::Program::instance_token = PTOK_INVAL + 1;

gaggled::Program::Program(std::string name, std::string command, std::vector<std::string>* argv, std::map<std::string, std::string> own_env, std::string wd, bool respawn, bool enabled) :
  name(name),
  command(command),
  argv(argv),
  wd(wd),
  own_env(own_env),
  respawn(respawn),
  operator_shutdown(!enabled),
  controlled_shutdown(false),
  running(false),
  prop_start(false),
  pid(0),
  token(PTOK_INVAL),
  statechanges(0)
{
  if (argv == NULL) {
    this->argv = new std::vector<std::string>();
  }
  dependencies = new std::vector<Dependency*>();

  if (argv) {
    if(!(exec_argv = (char**) malloc (sizeof(char*) * (argv->size() + 2))))
      exit(EX_OSERR);

    // argv[0] should be the command
    exec_argv[0] = strdup(this->command.c_str());
    if (exec_argv[0] == NULL)
      exit(EX_OSERR);

    // copy argv into a char**
    int _i = 1;
    for (auto i = argv->begin(); i != argv->end(); i++) {
      exec_argv[_i] = strdup(i->c_str());
      if (exec_argv[_i] == NULL)
        exit(EX_OSERR);
      _i++;
    }
    // terminate ARGV with a NULL ptr
    exec_argv[this->argv->size()+1] = NULL;
  }

  // create a null environment to start
  exec_env = (char**) malloc (sizeof(char*));
  exec_env[0] = NULL;
}

gaggled::Program::~Program() {
  for (int i = 0; exec_argv[i] != NULL; i++)
    free(exec_argv[i]);
  for (int i = 0; exec_env[i] != NULL; i++)
    free(exec_env[i]);
  free(exec_argv);
  free(exec_env);

  // just the vector, not the dependencies themselves
  delete dependencies;
  delete argv;
}

bool gaggled::Program::search(std::vector<std::string>* path) {
  // create list of commands
  if (strstr(exec_argv[0], "/") != NULL) {
    // just a command, no path stuff.
    commands.push_back(std::string(exec_argv[0]));
  } else {
    // get the command
    char* basename = exec_argv[0];
    for (auto p = path->begin(); p != path->end(); p++) {
      // prepend path
      commands.push_back(std::string(*p + "/" + basename));
    }
  }

  //buffer for storing stat() results
  struct stat st_buf;
  // check if at least one is executable and a file...
  for (auto c = commands.begin(); c != commands.end(); c++) {
    const char* fn = c->c_str();
    if (access(fn, X_OK) != 0)
      continue;
    // ensure that it's a regular file, not a directory
    stat(fn, &st_buf);
    if (S_ISREG(st_buf.st_mode))
      return true;
  }
  return false;
}

void gaggled::Program::overlay_environment(std::map<std::string, std::string> global_environment) {
  // overlay own over env from gaggled and global env{} block
  for (auto own = own_env.begin(); own != own_env.end(); own++)
    global_environment[own->first] = own->second;

  // delete the old env char**
  free(exec_env);

  // create a new c style one...
  exec_env = (char**) malloc (sizeof(char*) * (global_environment.size() + 1));
  if (exec_env == NULL)
    exit(EX_OSERR);

  int i = 0;
  for (auto en = global_environment.begin(); en != global_environment.end(); en++) {
    std::string entry = en->first + "=" + en->second;
    char* cpv = strdup(entry.c_str());
    if (cpv == NULL)
      exit(EX_OSERR);
    exec_env[i++] = cpv;
  }
  exec_env[global_environment.size()] = NULL;

  own_env = global_environment; //now that we've overlaid it... save it.
}

std::string gaggled::Program::to_string() {
  std::string r = "Program " + this->name + ": [" + this->command;
  for (auto i = this->argv->begin(); i != this->argv->end(); i++)
    r = r + " " + *i;

  r = r + "] respawn:" + boost::lexical_cast<std::string>(this->respawn) + " enabled:" + boost::lexical_cast<std::string>(!(this->operator_shutdown));
  r = r + " running:" + boost::lexical_cast<std::string>(this->running) + " pid:" + boost::lexical_cast<std::string>(this->pid);
  return r + " token:" + boost::lexical_cast<std::string>(this->token);
}

std::string gaggled::Program::getName() {
  return this->name;
}

void gaggled::Program::add_dependency(Dependency* d) {
  this->dependencies->push_back(d);
}

bool gaggled::Program::is_running() {
  return this->running;
}

pid_t gaggled::Program::get_pid() {
  return this->pid;
}

std::string gaggled::Program::getDownType() {
  return this->down_type;
}

uint64_t gaggled::Program::state_changes() {
  return this->statechanges;
}

std::string gaggled::Program::get_command() {
  return this->command;
}

bool gaggled::Program::dependencies_satisfied() {
  for (auto i = this->dependencies->begin(); i != this->dependencies->end(); i++) {
    if ((*i)->is_of(this) and not (*i)->satisfied()) {
      return false;
    }
  }
  return true;
}

void gaggled::Program::start(Gaggled* g) {
  if (not g->is_running()) {
    std::cout << "not starting " << name << ", gaggled is shutting down." << std::endl << std::flush;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // try to behave similarly to glibc execvpe
    bool err_perm = false;
    bool err_badbin = false;
    bool err_neverfound = true;

    if (wd != "") {
      if (chdir(wd.c_str()) != 0)
        {
        std::cout << "failed to chdir(\"" << wd << "\")... errno=" << errno << std::endl << std::flush;
        exit(EX_UNAVAILABLE);
        }
    }

    struct rlimit inf;
    inf.rlim_cur = RLIM_INFINITY;
    inf.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &inf);

    for (auto c = commands.begin(); c != commands.end(); c++) {
      exec_argv[0] = strdup(c->c_str());
      execve(exec_argv[0], exec_argv, exec_env);

      bool notfound = false;

      switch (errno) {
        case EACCES:
          // execve sets EACCESS when the file is not a regular file,
          // but this really should be counted as no binary found instead
          struct stat st_buf;
          stat(exec_argv[0], &st_buf);
          if (S_ISREG(st_buf.st_mode)) {
            err_perm = true;
          } else {
            notfound = true;
          }            
          break;
        case ENOENT:
        case ESTALE:
        case ENOTDIR:
        case ETIMEDOUT:
        case ENODEV:
          notfound = true;
          break;
        case ENOEXEC:
          err_badbin = true;
          break;
      }

      err_neverfound = err_neverfound and notfound;

      free(exec_argv[0]);
    }

    // In the case that this function is still executing, execv has failed every time. Try to figure out why, pass
    // back information about such failure to the parent via exit()
    if (err_perm) {
      exit(EX_NOPERM);
    } else if (err_neverfound) {
      exit(EX_NOINPUT);
    } else if (err_badbin) {
      exit(EX_DATAERR);
    } else {
      exit(EX_UNAVAILABLE);
    }
  } else if (pid == -1) {
    std::cout << "fork failed." << std::endl;
  } else {
    controlled_shutdown = false; // We're not in a controlled shutdown right now.  We just started, so that can't be true.
    g->pid_map[pid] = this;
    this->pid = pid;
    this->token = gaggled::Program::instance_token++;
    this->statechanges++;
    this->running = true;
    this->down_type = "UNK";
    this->prop_start = false;
    if (gettimeofday(&(this->started), NULL) != 0)
      std::cout << "error: failed to gettimeofday(), timing behaviour warning." << std::endl;

    std::cout << "forked for " << (*this) << std::endl;

    // broadcast the up state
    g->broadcast_state(this);
  }
}

void gaggled::Program::kill_program(Gaggled* g, int signal, bool prop_start, unsigned long long token) {
  if (not this->running) {
    if (prop_start) {
      // if it's already dead, then the died process has already passed.
      // there could already be a StartEvent queued, but we don't know that for sure.
      // best make another one.
      // the normal case where the died() function will create a restart is impossible here.
      new gaggled::StartEvent(g, this);
    }
    return;
  }

  // ok, this program is currently running...

  // if a token is supplied, only act if it matches
  if (token != 0)
    if (token != this->token)
      return;

  // program is running and are doing a prop kill: of course we set prop_start
  this->prop_start = prop_start;

  int kr = kill(this->pid, signal);
  if (kr == -1) {
    switch (errno) {
      case EINVAL :
        std::cout << "error: signal " << signal << " is invalid, could not kill " << this->pid << std::endl;
        break;
      case EPERM :
        std::cout << "error: no permission to signal child " << this->pid << std::endl;
        break;
      case ESRCH :
        break;
      default :
        std::cout << "error: unk errno " << errno << " from kill(" << this->pid << ", " << signal << ")" << std::endl;
        break;
    }
  } else {
    // this program is being shut down by gaggled (in some capacity) so the shutdown
    // is considered controlled.  If kill failed to send, then it shouldn't die, so we don't want
    // to mark it as controlled.
    controlled_shutdown = true;
    std::cout << "[gaggled] " << name << ": killing with signal " << signal << std::endl << std::flush;
  }
 
  return;
}

void gaggled::Program::died(Gaggled* g, std::string down_type, int rcode) {
  std::cout << "I died, says " << (*this) << std::endl;

  g->pid_map.erase(this->pid);
  this->pid = 0;
  this->running = false;
  this->down_type = down_type;
  this->token = PTOK_INVAL;
  this->statechanges++;

  switch (rcode) {
    case EX_NOPERM:
      std::cout << *this << " was not executable due to permissions." << std::endl;
      break;
    case EX_DATAERR:
      std::cout << *this << " executable format bad." << std::endl;
      break;
    case EX_NOINPUT:
      std::cout << *this << " file not found." << std::endl;
      break;
    case EX_UNAVAILABLE:
      std::cout << *this << " could not execute for an unknown reason." << std::endl;
      break;
  }

  // broadcast the down state
  g->broadcast_state(this);

  // if respawn and/or prop_start from a previous kill, start now
  if (this->prop_start) {
    // unflag
    this->prop_start = false;
    new gaggled::StartEvent(g, this);
  } else if (this->respawn) {
    new gaggled::StartEvent(g, this);
  }

  // So this program has died: doesn't matter why, if anything propagate=true depends
  // on this, we need to kill/restart it.
  for (auto i = this->dependencies->begin(); i != this->dependencies->end(); i++)
    if ((*i)->is_on(this))
      (*i)->prop_down(g);
}

uint64_t gaggled::Program::uptime() {
  if (not this->running)
    return 0;

  timeval uptime;
  if (gettimeofday(&uptime, NULL) != 0)
    std::cout << "error: failed to gettimeofday(), timing behaviour warning." << std::endl;

  uint64_t ms = (uptime.tv_sec - started.tv_sec) * 1000;
  ms += (uptime.tv_usec - started.tv_usec) / 1000;

  return ms;
}

bool gaggled::Program::is_up(int ms) {
  if (not this->running) {
    return false;
  }

  // we could do a difference by taking the differences between
  // now and this->started and removing them from the ms var, but we'd
  // overflow and end up thinking we didn't have enough uptime once 24.85 days
  // went by.
  // so instead, we'll wind back the clock to when we want the program
  // to have started by.
  timeval started_by;
  if (gettimeofday(&started_by, NULL) != 0)
    std::cout << "error: failed to gettimeofday(), timing behaviour warning." << std::endl;
  // first, whole seconds
  started_by.tv_sec -= ms / 1000;

  int usec_minus = (ms % 1000) * 1000;
  if (usec_minus <= started_by.tv_usec) {
    // straight up subtraction, easy!
    started_by.tv_usec -= usec_minus;
  } else {
    // we're wrapping around here. first, find out how far into the previous second
    // we're going, then assign a new usec value based on that.
    started_by.tv_usec = 1000000 - (usec_minus - started_by.tv_usec);
    // since we went to the previous second, we need to decrement that too.
    started_by.tv_sec--;
  }

  // now that we know when we want to have started by, check if we did.
  if (this->started.tv_sec < started_by.tv_sec) {
    // second is ahead
    return true;
  } else if (this->started.tv_sec == started_by.tv_sec and this->started.tv_usec <= started_by.tv_usec) {
    // in the same second, but the usec boundary has passed
    return true;
  }

  // nope, haven't been running long enough. better luck next time!
  return false;
}

unsigned long long gaggled::Program::get_token() {
  return this->token;
}

void gaggled::Program::op_start(gaggled::Gaggled* g) {
  operator_shutdown = false;
  this->statechanges++;
  g->broadcast_state(this);
  new StartEvent(g, this);
}

void gaggled::Program::op_kill(gaggled::Gaggled* g) {
  new KillEvent(g, this, SIGTERM, false, false);
}

void gaggled::Program::op_shutdown(gaggled::Gaggled* g) {
  operator_shutdown = true;
  this->statechanges++;
  g->broadcast_state(this);
  g->flush_starts(this);
  new KillEvent(g, this, SIGTERM, false, false);
}

bool gaggled::Program::is_operator_shutdown() {
  return operator_shutdown;
}

bool gaggled::Program::is_controlled_shutdown() {
  return controlled_shutdown;
}

std::vector<gaggled::Dependency*> gaggled::Program::get_dependencies() {
  std::vector<gaggled::Dependency*> outbound;

  for (auto d = dependencies->begin(); d != dependencies->end(); d++)
    if ((*d)->is_of(this))
      outbound.push_back(*d);

  return outbound;
}

std::ostream& gaggled::operator<< (std::ostream &stream, gaggled::Program& p) {
  stream << (&p)->to_string();
  return stream;
}
