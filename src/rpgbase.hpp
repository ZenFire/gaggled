#ifndef RPGBASE_H_INCLUDED
#define RPGBASE_H_INCLUDED

/* Auto-Generated by RPG. Don't edit this unless you really know what you're up to. No, really. */

#include <stdint.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <zmq.hpp>
#include "rpgbase.hpp"

namespace rpgbase {

    class RPGService {
    public:
     // functions
      RPGService () {
      }
      virtual void run_once_bare () {
      }
     // members
      zmq::context_t* ctx;
      zmq::socket_t* sock;
      zmq::pollitem_t pollitem;
    };

    template<typename implementation_child_type>
    class RPGMulti {
    public:
     // functions
      RPGMulti (RPGService** services, uint32_t n) : pollset_n(n), services_a(services) {
        pollset = new zmq::pollitem_t[n];
        for (uint32_t i=0; (i < n); i = (i + 1)) {
          pollset[i].socket = (*((*(services[i])).sock));
          pollset[i].events = ZMQ_POLLIN;
        }
      }
      ~RPGMulti () {
        delete[] pollset;
        delete[] services_a;
      }
      void run_once (long timeout=0) {
        uint32_t n_busy=0;
        try {
          n_busy = zmq::poll(pollset, pollset_n, timeout);
        } catch (zmq::error_t& ze) {
          return;
        }
        for (uint32_t i=0; ((i < pollset_n) and (n_busy > 0)); i = (i + 1)) {
          if ((pollset[i].revents == ZMQ_POLLIN)) {
            n_busy = (n_busy - 1);
            (services_a[i])->run_once_bare();
          }
        }
      }
     // members
      zmq::pollitem_t* pollset;
      uint32_t pollset_n;
      RPGService** services_a;
    };
}
#endif