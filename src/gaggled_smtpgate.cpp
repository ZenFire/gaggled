#include <sys/time.h>

#include "time.h"

#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <memory>

#include "gaggled_events_client.hpp"
#include "gaggled_control_client.hpp"
#include "util/smtp.hpp"
#include "gv.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>

size_t min(size_t a, size_t b)
  {
  if (b > a)
    return a;
  else
    return b;
  }

class ProgramStateTracker {
public:
const static uint8_t STATE_NONE = 0;
const static uint8_t STATE_UP = 1;
const static uint8_t STATE_DOWN = 2;
const static uint8_t STATE_CHURN = 3;
const static uint8_t STATE_OPDOWN = 4;

typedef std::pair<boost::system_time, gaggled_events_client::ProgramState> stamped_state;

  ProgramStateTracker(std::string name, bool verbose) : name(name), last_known_pid(0), announced_state(STATE_NONE), verbose(verbose) {
    announced_program_state.program = name;
    announced_program_state.state_sequence = 0;
    announced_time = boost::get_system_time();
  }

  void find_last_pid(bool skip_last=false) {
    int i = 0;
    for (auto st = inbound_program_states.rbegin(); st != inbound_program_states.rend(); st++) {
      if (st->second.up == 1 && !(skip_last && i == 0)) {
        last_known_pid = st->second.pid;
        break;
      }
      i++;
    }
  }

  // returns a state change to emit, if any should be.
  uint8_t notify(int period) {
    // Nothing has changed. How could we have anything to say?
    if (inbound_program_states.size() == 0) {
      return STATE_NONE;
    }

    // We talked about an event less than a short while ago... wait more before deciding what to say.
    if (announced_time + boost::posix_time::milliseconds(period) > boost::get_system_time()) {
      return STATE_NONE;
    }

    stamped_state first = *(inbound_program_states.begin());
    stamped_state last = *(inbound_program_states.rbegin());

    // !OPDOWN -> OPDOWN?
    if (announced_state != STATE_OPDOWN) {
      if ((last.second.up == 0) && (last.second.is_operator_shutdown == 1)) {
        // What did we base our last statement on, and when did we get it?
        announced_time = last.first;
        announced_program_state = last.second;

        // try to find the last used pid in the inbound states before clearing
        find_last_pid();

        // throw everything away: an operator shut it down and that's what's important.
        inbound_program_states.clear();
        return (announced_state = STATE_OPDOWN);
      }
    }

    // NONE -> (UP | DOWN)?
    if (announced_state == STATE_NONE) {
      // What did we base our last statement on, and when did we get it?
      announced_time = first.first;
      announced_program_state = first.second;
      // throw away just the first event.
      inbound_program_states.erase(inbound_program_states.begin());

      // store and announce.
      if (announced_program_state.up == 1)
        {
        last_known_pid = announced_program_state.pid;
        announced_state = STATE_UP;
        }
      else
        {
        return (announced_state = STATE_DOWN);
        }

      return STATE_NONE;
    }

    // !CHURN -> CHURN?
    /*
     * CHURN indicates that the program has crashed more than once.  Incorrectly stating that a program is in CHURN can result in 
     * confusion for the sysadmin; so we must condition entering CHURN on there being more than two state changes; just two could indicate
     * a program crashed once and recovered.
     */
    if (announced_state != STATE_CHURN) {
      int states = inbound_program_states.size();
      if (states > 2) {
        int ups = 0;
        int downs = 0;

        for (int i = 0; i < states - 1; i++) {
          if (inbound_program_states[i].second.up == 0)
            downs++;
          else
            ups++;
        }

        if (ups > 0 && downs > 0) {
          // What did we base our last statement on, and when did we get it?
          announced_time = inbound_program_states[states - 2].first;
          announced_program_state = inbound_program_states[states - 2].second;

          // try to find the last used pid in the inbound states before clearing
          find_last_pid(true);

          // save only the latest (unprocessed) event
          inbound_program_states.clear();
          inbound_program_states.push_back(last);
          // we are now considered to be churning: it's going up and down a decent bit.
          return (announced_state = STATE_CHURN);
        }
      }
    }

    // 'non-churning' transitions to UP and DOWN steady states
    {
      // UP -> DOWN?
      if (announced_state == STATE_UP && first.second.up == 0) {
        // no timing check needed here: it's gone down.
        announced_time = first.first;
        announced_program_state = first.second;

        // just remove this first one
        inbound_program_states.erase(inbound_program_states.begin());

        return (announced_state = STATE_DOWN);
      }

      // (DOWN | OPDOWN) -> UP?
      if ((announced_state == STATE_DOWN || announced_state == STATE_OPDOWN) && last.second.up == 1) {
        // here, we need to know it's been up for a bit before we can state that it's up. condition on age of last event.

        if (last.first + boost::posix_time::milliseconds(period) < boost::get_system_time()) {
          announced_time = last.first;
          announced_program_state = last.second;

          // try to find the last used pid in the inbound states before clearing
          find_last_pid();

          inbound_program_states.clear();
          return (announced_state = STATE_UP);
        }
      }
      // OPDOWN -> DOWN?
      else if (announced_state == STATE_OPDOWN && last.second.up == 0 && last.second.is_operator_shutdown == 0) {
        announced_time = last.first;
        announced_program_state = last.second;

        // try to find the last used pid in the inbound states before clearing
        find_last_pid();

        inbound_program_states.clear();
        return (announced_state = STATE_DOWN);
      }
    }

    // CHURN -> (UP | DOWN)    
    if (announced_state == STATE_CHURN) {
      if (last.first + boost::posix_time::milliseconds(period) < boost::get_system_time()) {
        // What did we base our last statement on, and when did we get it?
        announced_time = last.first;
        announced_program_state = last.second;

        // try to find the last used pid in the inbound states before clearing
        find_last_pid();

        inbound_program_states.clear();

        // store and announce.
        if (announced_program_state.up == 1)
          announced_state = STATE_UP;
        else
          announced_state = STATE_DOWN;

        return announced_state;
      } else {
        // try to find the last used pid in the inbound states before clearing
        find_last_pid(true);

        // we haven't stabilized but since we're already in CHURN state we can just discard everything up until the end and nothing's changed.
        inbound_program_states.clear();
        // after clearing, put the last back on the list again.
        inbound_program_states.push_back(last);
        return STATE_NONE;
      }
    }

    size_t states = inbound_program_states.size();
    // we were unable to decide anything... give up.
    if (verbose) std::cout << "couldn't decide what to say for " << name << " (queued states=" << inbound_program_states.size() << "), so saying nothing." << std::endl;
    if (verbose && states > 0)
      {
      size_t show = min(20, states);
      std::cout << "last " << show << " states (newest last):" << std::endl;
      for (size_t i = states - show; i < states; i++)
        {
        std::cout << "program=" << inbound_program_states[i].second.program;
        std::cout << " dependencies_satisfied=" << int(inbound_program_states[i].second.dependencies_satisfied);
        std::cout << " is_operator_shutdown=" << int(inbound_program_states[i].second.is_operator_shutdown);
        std::cout << " during_shutdown=" << int(inbound_program_states[i].second.during_shutdown);
        std::cout << " state_sequence=" << inbound_program_states[i].second.state_sequence;
        std::cout << " down_type=" << inbound_program_states[i].second.down_type;

        std::cout << " uptime_ms=" << inbound_program_states[i].second.uptime_ms;
        std::cout << " up=" << int(inbound_program_states[i].second.up);

        std::cout << std::endl;
        }
      }
    return STATE_NONE;
  }

  void update(gaggled_events_client::ProgramState& ps) {
    int states = inbound_program_states.size();
    auto st = stamped_state(boost::get_system_time(), ps);

    if (states == 0) {
      // this is the only state, go ahead and add it.
      inbound_program_states.push_back(st);
      if (verbose) std::cout << "initial state for " << name << " (seqid=" << ps.state_sequence << ")" << std::endl;
    } else {
      if (announced_program_state.state_sequence >= ps.state_sequence) {
        if (verbose) std::cout << "already-processed dropped state for " << name << " (seqid=" << ps.state_sequence << ")" << std::endl;
        return;
      }

      if (inbound_program_states.rbegin()->second.state_sequence < ps.state_sequence) {
        // this is the normal case, it's in sequence and we can just push_back.
        inbound_program_states.push_back(st);
        if (verbose) std::cout << "appended state for " << name << " (seqid=" << ps.state_sequence << ")" << std::endl;
        return;
      }

      auto insert_at = inbound_program_states.end();
      for (auto p = inbound_program_states.begin(); p != inbound_program_states.end(); p++) {
        if (ps.state_sequence == p->second.state_sequence) {
          if (verbose) std::cout << "dupe-dropped state for " << name << " (seqid=" << ps.state_sequence << ")" << std::endl;
          return; // nope, it's a dupe. toss it!
        }

        if (ps.state_sequence < p->second.state_sequence)
          insert_at = p;
      }
      if (insert_at != inbound_program_states.end()) {
        inbound_program_states.insert(insert_at, st);
        if (verbose) std::cout << "inserted state for " << name << " (seqid=" << ps.state_sequence << ")" << std::endl;
      }
    }
  }

  uint8_t get_state() {
    return announced_state;
  }

  void update(gaggled_control_client::ProgramState& ps) {
    gaggled_events_client::ProgramState pe;

    pe.program = ps.program;
    pe.up = ps.up;
    pe.dependencies_satisfied = ps.dependencies_satisfied;
    pe.is_operator_shutdown = ps.is_operator_shutdown;
    pe.during_shutdown = ps.during_shutdown;
    pe.state_sequence = ps.state_sequence;
    pe.down_type = ps.down_type;
    pe.pid = ps.pid;
    pe.uptime_ms = ps.uptime_ms;

    update(pe);
  }

  inline int64_t get_last_known_pid() {
    return last_known_pid;
  }
  
private:
  std::string name;
  int64_t last_known_pid;

  // latest announced/processed state information
  uint8_t announced_state;
  gaggled_events_client::ProgramState announced_program_state;
  // this is the timestamp that we had on the event that caused us to talk, not when we said it. Events can be delayed by throttled.
  boost::system_time announced_time;
  
  // timestamped state events
  std::vector<stamped_state> inbound_program_states;

  // verbosity
  bool verbose;
};

class Transition {
public:
const static uint8_t CHG_NONE = 0;
const static uint8_t CHG_BETTER = 1;
const static uint8_t CHG_WORSE = 2;
const static uint8_t CHG_NEUT = 3;

  Transition(uint8_t from, uint8_t to, std::string program, int64_t last_known_pid)
  : from(from)
  , to(to)
  , program(program)
  , last_known_pid(last_known_pid)
  {
    if (to == ProgramStateTracker::STATE_CHURN) {
      chg = CHG_WORSE;
    } else if (to == ProgramStateTracker::STATE_UP) {
      chg = CHG_BETTER;
    } else if (to == ProgramStateTracker::STATE_DOWN) {
      if (from == ProgramStateTracker::STATE_CHURN) {
        chg = CHG_BETTER;
      } else {
        chg = CHG_WORSE;
      }
    } else if (to == ProgramStateTracker::STATE_OPDOWN) {
      chg = CHG_NEUT;
    } else {
      // nope
      chg = CHG_NONE;
    }
  }

  std::string breakdown(bool multiline=false) {
    std::string summary = "process " + program + " is ";
    switch (to) {
      case ProgramStateTracker::STATE_CHURN:
        summary += "churning";
        break;
      case ProgramStateTracker::STATE_UP:
        summary += "up";
        break;
      case ProgramStateTracker::STATE_DOWN:
        summary += "down";
        break;
      case ProgramStateTracker::STATE_OPDOWN:
        summary += "operator disabled";
        break;
      default :
        summary += "unknown";
        break;
    }

    if (from != ProgramStateTracker::STATE_NONE) {
      if (multiline)
        summary += "\r\n  ";
      else
        summary += ", ";
      
      switch (from) {
        case ProgramStateTracker::STATE_CHURN:
          summary += "was churning";
          break;
        case ProgramStateTracker::STATE_UP:
          summary += "was up";
          break;
        case ProgramStateTracker::STATE_DOWN:
          summary += "was down";
          break;
        case ProgramStateTracker::STATE_OPDOWN:
          summary += "was operator disabled";
          break;
      }
    }


    if (last_known_pid != 0) {
      if (multiline)
        summary += "\r\n  ";
      else
        summary += ", ";
      
      if (to != ProgramStateTracker::STATE_UP)
        summary += "last known pid";
      else
        summary += "current pid";
      
      summary += ": " + boost::lexical_cast<std::string>(last_known_pid); 
    }


    return summary;
  }

  uint8_t from;
  uint8_t to;
  std::string program;
  int64_t last_known_pid;

  uint8_t chg;
  bool verbose;
};

class StateNotifier {
private:
  void setup(std::string program_name) {
    if (program_trackers.find(program_name) == program_trackers.end())
      program_trackers[program_name] = std::shared_ptr<ProgramStateTracker>(new ProgramStateTracker(program_name, verbose));
  }
public:
  StateNotifier(int state_period, int batch_period, std::string prefix, std::string mx, std::string helo, std::string from, std::string to, bool verbose)
    : agent(mx, helo)
    , state_period(state_period)
    , batch_period(batch_period)
    , prefix(prefix)
    , from(from)
    , to(to)
    , verbose(verbose)
  {
    batched_transmission_time = boost::get_system_time();
  }

  void update(gaggled_events_client::ProgramState& ps) {
    setup(ps.program);
    program_trackers[ps.program]->update(ps);
  }

  void update(gaggled_control_client::ProgramState& ps) {
    setup(ps.program);
    program_trackers[ps.program]->update(ps);
  }

  void mail_send(std::string from, std::string to, std::string subject, std::string body) {
    try {
      agent.send(from, to, subject, body);
    } catch (gaggled::util::SMTPHostException& se) {
      std::cout << "gaggled_smtpgate: [smtp] unknown host" << std::endl;
    }  catch (gaggled::util::SMTPConnectionRefusedException& se) {
      std::cout << "gaggled_smtpgate: [smtp] connection refused" << std::endl;
    } catch (gaggled::util::SMTPConnectionTimeoutException& se) {
      std::cout << "gaggled_smtpgate: [smtp] connection timeout" << std::endl;
    } catch (gaggled::util::SMTPConnectionDroppedException& se) {
      std::cout << "gaggled_smtpgate: [smtp] connection dropped" << std::endl;
    } catch (gaggled::util::SMTPConnectionException& se) {
      std::cout << "gaggled_smtpgate: [smtp] connection failure" << std::endl;
    } catch (gaggled::util::SMTPBadMailboxOrDomainException& se) {
      std::cout << "gaggled_smtpgate: [smtp] bad mailbox or domain. " << se.what() << std::endl;
    } catch (gaggled::util::SMTPTooSlowException& se) {
      std::cout << "gaggled_smtpgate: [smtp] server too slow, deadline was " << agent.ms_deadline() << "ms. " << se.what() << std::endl;
    } catch (gaggled::util::SMTPNoRelayException& se) {
      std::cout << "gaggled_smtpgate: [smtp] smtp relaying refused. " << se.what() << std::endl;
    } catch (gaggled::util::SMTPRejectedException& se) {
      std::cout << "gaggled_smtpgate: [smtp] message rejected. " << se.what() << std::endl;
    } catch (gaggled::util::SMTPProtocolException& se) {
      std::cout << "gaggled_smtpgate: [smtp] protocol violation from server. " << se.what() << std::endl;
    } catch (gaggled::util::SMTPException& se) {
      std::cout << "gaggled_smtpgate: [smtp] unknown problem" << std::endl;
    } catch (std::exception& se) {
      std::cout << "gaggled_smtpgate: [smtp] unknown problem (caught std::exception)" << std::endl;
    }
  }

  void notify() {
    if (batch_period == 0) {
      for (auto p = program_trackers.begin(); p != program_trackers.end(); p++) {
        std::string program = p->first;
        auto tracker = p->second;
        uint8_t previous_state = tracker->get_state();
        uint8_t new_state = tracker->notify(state_period);
        if (new_state != ProgramStateTracker::STATE_NONE) {
          auto breakdown = Transition(previous_state, new_state, program, tracker->get_last_known_pid()).breakdown();
          std::cout << "gaggled_smtpgate: [state] " << breakdown << std::endl;
          mail_send(from, to, prefix + breakdown, breakdown);
        }
      }
    } else {
      for (auto p = program_trackers.begin(); p != program_trackers.end(); p++) {
        std::string program = p->first;
        auto tracker = p->second;
        uint8_t previous_state = tracker->get_state();

        // if we already have something to say about this program, this isn't going anywhere.
        if (programs_batched.find(program) != programs_batched.end())
          continue;

        uint8_t new_state = tracker->notify(state_period);
        if (new_state != ProgramStateTracker::STATE_NONE) {
          programs_batched.insert(program);
          transitions.push_back(Transition(previous_state, new_state, program, tracker->get_last_known_pid()));
        }
      }

      bool all_up = true;
      for (auto p = program_trackers.begin(); p != program_trackers.end(); p++)
        all_up &= (p->second->get_state() == ProgramStateTracker::STATE_UP);

      if (!(programs_batched.empty()) && (batched_transmission_time <= boost::get_system_time() - boost::posix_time::milliseconds(batch_period))) {
        // send mail, flush set/vec, set ts
        int bad_cnt = 0;
        int good_cnt = 0;
        int neut_cnt = 0;

        std::string body = "There are " + boost::lexical_cast<std::string>(transitions.size()) + " state change(s) of note:\r\n";

        for (auto trans = transitions.begin(); trans != transitions.end(); trans++) {
          uint8_t chg = trans->chg;
          switch (chg) {
            case Transition::CHG_BETTER:
              good_cnt++;
              break;
            case Transition::CHG_WORSE:
              bad_cnt++;
              break;
            case Transition::CHG_NEUT:
              neut_cnt++;
              break;
          }

          body += "\r\n" + trans->breakdown(true);
        }

        std::string subject = prefix;
        if (bad_cnt > 0 && good_cnt > 0) {
          // mixed bag
          subject += " mixed changes";
        } else if (bad_cnt > 0) {
          // things are bad!
          subject += " degradation";
        } else if (good_cnt > 0) {
          // things are good!
          subject += " improvement";
        } else if (neut_cnt > 0) {
          // things are neutral overall.
          subject += " neutral changes";
        } else {
          // all is confusion!
          subject += " unknown state changes";
        }

        if (all_up)
          subject += " [all ok]";

        batched_transmission_time = boost::get_system_time();

        mail_send(from, to, subject, body);

        programs_batched.clear();
        transitions.clear();
      }
    }
  }

private:
  std::map<std::string, std::shared_ptr<ProgramStateTracker>> program_trackers;
  gaggled::util::SMTP agent;
  int state_period;
  int batch_period;
  std::string prefix;
  std::string from;
  std::string to;
  bool verbose;
  
  // batching state variables. when the last transmission was made, what transitions are waiting to be transmitted if any, and which programs are waiting.
  boost::system_time batched_transmission_time;
  std::set<std::string> programs_batched;
  std::vector<Transition> transitions;
};

class GaggledWatcher : public gaggled_events_client::gaggled_events<GaggledWatcher> {
  public:
  GaggledWatcher(const char* url, StateNotifier& state)
    : gaggled_events_client::gaggled_events<GaggledWatcher>(url)
    , waiting_on(-1)
    , got_response(false)
    , state(state)
  {
  }

  void handle_statechange(gaggled_events_client::ProgramState& obj) {
    state.update(obj);
  }

  void handle_dumped(int32_t req) {
    if (req == waiting_on) {
      got_response = true;
    }
  }

  int32_t waiting_on;
  bool got_response;

  private:
    StateNotifier& state;
};

void usage() {
  std::cout << "gaggled_smtpgate v" << gaggled::version << ", the SMTP notification gateway for gaggled." << std::endl << std::endl;
  std::cout << "usage: gaggled_smtpgate (-h|-c <file> -u <url> -m <mx> -f <from> -t <to>, -s <helo>)" << std::endl;
  std::cout << "\t-c <file> where the gaggled config file is located." << std::endl;
  std::cout << "\t-v to run in verbose mode." << std::endl << std::endl;
  std::cout << "\t-h to show help." << std::endl << std::endl;
  std::cout << "All the settings to use must be in the gaggled config file specified." << std::endl << std::endl;
  std::cout << "REQUIRED SETTINGS" << std::endl << std::endl;
  std::cout << "gaggled {" << std::endl;
  std::cout << "  controlurl and eventurl - both must be active for gaggled_smtpgate to work." << std::endl;
  std::cout << "  smtpgate {" << std::endl;
  std::cout << "    mx - hostname of SMTP server" << std::endl;
  std::cout << "    helo - HELO identification to present to the server" << std::endl;
  std::cout << "    spfix - prefix for email subjects" << std::endl;
  std::cout << "    from - email address to send from" << std::endl;
  std::cout << "    to - email address to send to" << std::endl;
  std::cout << "  }" << std::endl;
  std::cout << "}" << std::endl << std::endl;
  std::cout << "OPTIONAL SETTINGS" << std::endl << std::endl;
  std::cout << "gaggled {" << std::endl;
  std::cout << "  smtpgate {" << std::endl;
  std::cout << "    period - must wait this many milliseconds before deciding a new state since the last one (default 10000)" << std::endl;
  std::cout << "    batch - if set, activate batch mode and must wait this many milliseconds before sending out new changes since the last. Batch mode includes an [ALL OK] flag in the subject when recovery is complete." << std::endl;
  std::cout << "    auto - boolean. Does not affect gaggled_smtpgate directly; if true, gaggled will start an smtpgate called smtpgate without any config stanza for the smtpgate subprocess." << std::endl;
  std::cout << "  }" << std::endl;
  std::cout << "}" << std::endl;
}

#define SMTP_CONFIGTEST_FAIL(msg) std::cout << msg << std::endl << std::flush; \
        return 1;

int main(int argc, char** argv) {
  const char* conf_file = NULL;
  bool verbose = false;
  bool help = false;

  int c;
  while ((c = getopt(argc, argv, "hvc:")) != -1) {
    switch(c) {
      case 'h':
        help = true;
        break;
      case 'v':
        verbose = true;
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
    usage();
    return 0;
  }
  
  if (conf_file == NULL) {
    usage();
    return 2;
  }

  #include "gaggled_smptgate_config.hpp"

  StateNotifier gaggled_state(period, batch, prefix, mx, helo, from, to, verbose);
  
  std::shared_ptr<gaggled_control_client::gaggled_control> ctrl(new gaggled_control_client::gaggled_control(controlurl.c_str()));
  std::shared_ptr<GaggledWatcher> gw(new GaggledWatcher(eventurl.c_str(), gaggled_state));

  boost::system_time loop_end = boost::get_system_time() + boost::posix_time::milliseconds(5000);
  while (boost::get_system_time() < loop_end) {
    gw->run_once(50000);
  }

  timeval tv;
  if (gettimeofday(&(tv), NULL) != 0) {
    std::cout << "gaggled_smtpgate: error: failed to gettimeofday(), timing behaviour warning." << std::endl << std::flush;
    std::exit(1);
  }

  gw->waiting_on = tv.tv_usec;

  auto states = ctrl->call_getstates(gw->waiting_on);
  for (auto ps = states.begin(); ps != states.end(); ps++)
    gaggled_state.update(*ps);

  loop_end = boost::get_system_time() + boost::posix_time::milliseconds(5000);
  while (boost::get_system_time() < loop_end) {
    gw->run_once(50000);
  }

  if (!(gw->got_response)) {
    std::cout << "gaggled_smtpgate: error: it appears that the eventurl channel did not start within 5-10 seconds, aborting." << std::endl << std::flush;
    std::exit(1);
  } else {
    std::cout << "gaggled_smtpgate: synchronized state successfully" << std::endl << std::flush;
  }    

  uint64_t seq = 0;
  while(1) {
    seq++;
    gw->run_once(50000);
    if (seq % 20 == 0) {
      // every second, maybe notify.
      gaggled_state.notify();
    }
  }

  return 0;
}
