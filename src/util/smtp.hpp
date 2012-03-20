#ifndef GAGGLED_UTIL_SMTP_HPP_INCLUDED
#define GAGGLED_UTIL_SMTP_HPP_INCLUDED

#include <exception>

#include <boost/thread/thread_time.hpp>

namespace gaggled {
namespace util {

class SMTPException : public virtual std::exception
  {
public:
  virtual
  const char*
  what() const throw();

public:
  SMTPException(std::string msg) throw () ;
  SMTPException() throw () ;
  virtual ~SMTPException() throw() {};

protected:
  std::string msg;
  };

  class SMTPHostException : virtual SMTPException {};

  class SMTPSocketException : virtual SMTPException {};

  class SMTPConnectionException : virtual SMTPException {};
    class SMTPConnectionRefusedException : virtual SMTPConnectionException {};
    class SMTPConnectionTimeoutException : virtual SMTPConnectionException {};
    class SMTPConnectionDroppedException : virtual SMTPConnectionException {};

  class SMTPRejectedException : public SMTPException {
public:
  SMTPRejectedException(std::string msg);
  };
    class SMTPNoRelayException : public SMTPRejectedException {
  public:
    SMTPNoRelayException(std::string msg);
    };
    class SMTPBadMailboxOrDomainException : public SMTPRejectedException {
  public:
    SMTPBadMailboxOrDomainException(std::string msg);
    };

  class SMTPProtocolException : public SMTPException {
  public:
    SMTPProtocolException(std::string msg);
    };

  class SMTPTooSlowException : virtual SMTPException {};

  class DeadlineException : virtual SMTPException {};

class SMTP
  {
public:
  SMTP(std::string mx, std::string helo);
  void send(std::string from, std::string to, std::string subject, std::string body);

private:
  void send_socket(int sock, std::string s, boost::system_time deadline);
  std::string read_line(int sock, boost::system_time deadline);
  int rcode(std::string line);
  void no_hangup(int v);
  void in_range(int v, int begin, int end, std::string line);

  std::string mx;
  std::string helo;
  boost::posix_time::time_duration total_transaction_max;
  };

}}

#endif
