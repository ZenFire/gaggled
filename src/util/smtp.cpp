#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>
#include <exception>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "smtp.hpp"

gaggled::util::SMTPException::SMTPException(std::string msg) throw() :
  msg(msg)
  {
  }

gaggled::util::SMTPException::SMTPException() throw() {}

const char* gaggled::util::SMTPException::what() const throw()
  {
  return msg.c_str();
  }

gaggled::util::SMTPRejectedException::SMTPRejectedException(std::string msg) : SMTPException(msg) {}

gaggled::util::SMTPBadMailboxOrDomainException::SMTPBadMailboxOrDomainException(std::string msg) : SMTPRejectedException(msg) {}
gaggled::util::SMTPNoRelayException::SMTPNoRelayException(std::string msg) : SMTPRejectedException(msg) {}

gaggled::util::SMTPProtocolException::SMTPProtocolException(std::string msg) : SMTPException(msg) {}

gaggled::util::SMTP::SMTP(std::string mx, std::string helo)
  :
  mx(mx),
  helo(helo),
  total_transaction_max(boost::posix_time::milliseconds(30000))
  {
  }

class fd
  {
public:
  fd(int fd) : _fd(fd) {}
  ~fd()
    {
    close(_fd);
    }
private:
  int _fd;
  };

void gaggled::util::SMTP::deadline_wr_timeval(boost::system_time &deadline, struct timeval &tv)
  {
  boost::system_time now = boost::get_system_time();
  if (deadline > now)
    {
    boost::posix_time::time_duration rem = deadline - now;
    tv.tv_sec = rem.total_seconds();
    tv.tv_usec = rem.total_microseconds();
    }
  else
    {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    }
  }

void gaggled::util::SMTP::send_socket(int sock, std::string s, boost::system_time deadline)
  {
  int len = s.length();
  int offset = 0;
  const char* buf = s.c_str();

	struct timeval tv;
  fd_set writefd;

  while (offset < len)
    {
    deadline_wr_timeval(deadline, tv);

	  FD_ZERO(&writefd);
  	FD_SET(sock, &writefd);
  	if (select(sock+1, NULL, &writefd, NULL, &tv) == -1)
      {
      if (errno == EBADF)
        throw gaggled::util::SMTPConnectionDroppedException();
      else
        throw gaggled::util::SMTPConnectionException();
      }

    if (FD_ISSET(sock, &writefd))
      {
      int r = write(sock, buf + offset, len - offset);
      if (r == -1)
        {
        throw gaggled::util::SMTPConnectionDroppedException();
        }
      else
        {
        offset += r;
        }
      }
    
    if (boost::get_system_time() > deadline)
      throw DeadlineException();
    }

  //std::cout << "sent \"" << s << "\"" << std::endl;
  }

long gaggled::util::SMTP::ms_deadline()
  {
  return total_transaction_max.total_milliseconds();
  }

std::string gaggled::util::SMTP::read_line(int sock, boost::system_time deadline)
  {
  int bytes = 65536;
  char buf[65636];

	struct timeval tv;
  fd_set readfd;

  int recvd = 0;
  int bytes_skip = 0;

  while (recvd < bytes)
    {
    deadline_wr_timeval(deadline, tv);

	  FD_ZERO(&readfd);
  	FD_SET(sock, &readfd);
  	if (select(sock+1, &readfd, NULL, NULL, &tv) == -1)
      {
      if (errno == EBADF)
        throw gaggled::util::SMTPConnectionDroppedException();
      else
        throw gaggled::util::SMTPConnectionException();
      }

    if (FD_ISSET(sock, &readfd))
      {
      // TODO optimize to not take one byte at a time, keep a buffer going between read_line calls
      int r = read(sock, buf + recvd, 1);
      if (r == -1)
        {
        throw gaggled::util::SMTPConnectionDroppedException();
        }
      else
        {
        recvd += r;
        }

      if (bytes_skip < recvd)
        {
        if (buf[recvd - 2] == 0x0D && buf[recvd - 1] == 0x0A)
          {
          buf[recvd - 2] = 0;
          std::string r(buf);
          //std::cout << "got back line " << r << std::endl;
          return r;
          }
        }
      }
    
    if (boost::get_system_time() > deadline)
      throw DeadlineException();
    }

  throw gaggled::util::SMTPConnectionException();
  }

int gaggled::util::SMTP::rcode(std::string line)
  {
  size_t nlen = line.find(' ');
  
  if (nlen != std::string::npos)
    {
    if (nlen > 3 || nlen < 1)
      throw SMTPProtocolException("server said: " + line);

    // it's not too big and it's an integer, check the chars.
    for (size_t i = 0; i < nlen; i++)
      if (line[i] < '0' || line[i] > '9')
        throw SMTPProtocolException("server said: " + line);

    // so we have a number we can use.
    return atoi(line.substr(0, nlen).c_str());
    }

  throw SMTPProtocolException("server said: " + line);
  }

void gaggled::util::SMTP::no_hangup(int v)
  {
  if (v == 221)
    throw SMTPConnectionDroppedException();
  }

void gaggled::util::SMTP::in_range(int v, int begin, int end, std::string line)
  {
  if (v < begin || v >= end)
    {
    if (v == 554 || v == 551)
      throw SMTPNoRelayException("server said: " + line);

    if (v == 521 || v == 553)
      throw SMTPBadMailboxOrDomainException("server said: " + line);

    throw SMTPProtocolException("server said: " + line);
    }
  }

void gaggled::util::SMTP::send(std::string from, std::string to, std::string subject, std::string body)
  {
  boost::system_time deadline = boost::get_system_time() + total_transaction_max;
  
  struct sockaddr_in server;
  struct hostent *hp;
  
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (sock == -1)
    throw gaggled::util::SMTPSocketException();

  fd fdsock(sock);

  server.sin_family = AF_INET;

  // TODO DNS timeout?
  
  hp = gethostbyname(mx.c_str());
  if (hp==(struct hostent *) 0)
    throw gaggled::util::SMTPHostException();

  // copy the address to our sockaddr_in, set port
  memcpy((char *) &server.sin_addr, (char *) hp->h_addr, hp->h_length);
  server.sin_port=htons(25);

  if (connect(sock, (struct sockaddr *) &server, sizeof server) == -1)
    {
    if (errno == ETIMEDOUT)
      throw gaggled::util::SMTPConnectionTimeoutException();
    if (errno == EINPROGRESS)
      {
    	struct timeval tv;
      fd_set writefd;

      while (boost::get_system_time() < deadline)
        {
        deadline_wr_timeval(deadline, tv);

      	FD_ZERO(&writefd);
      	FD_SET(sock, &writefd);

      	if (select(sock+1, NULL, &writefd, NULL, &tv) == -1)
          {
          if (errno == EBADF)
            throw gaggled::util::SMTPConnectionDroppedException();
          else if (errno == ETIMEDOUT)
            throw gaggled::util::SMTPConnectionTimeoutException();
          else
            throw gaggled::util::SMTPConnectionException();
          }

        if (FD_ISSET(sock, &writefd))
          {
          // this means that at least a result is to be had, getsockopt to find the status
          int sock_ov = -1;
          int sock_ov_len = sizeof(sock_ov);
          if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (int*)&sock_ov, (socklen_t*)&sock_ov_len) != 0)
            {
            throw gaggled::util::SMTPConnectionException();
            }

          if (sock_ov == 0)
            {
            goto connected;
            }
          else
            {
            if (sock_ov == ECONNREFUSED)
              throw gaggled::util::SMTPConnectionRefusedException();
            if (sock_ov == EINPROGRESS)
              throw gaggled::util::SMTPConnectionTimeoutException();

            throw gaggled::util::SMTPConnectionException();
            }
          }
        } throw gaggled::util::SMTPConnectionTimeoutException();
      }
    }

  connected:

  try
    {
    std::string line;
    int rc;

    send_socket(sock, "HELO " + helo + "\r\n", deadline);
    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc); in_range(rc, 200, 300, line);

    send_socket(sock, "MAIL FROM: " + from + "\r\n", deadline);
    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc); in_range(rc, 200, 300, line);

    send_socket(sock, "VRFY " + to + "\r\n", deadline);
    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc);
    if (rc != 551)
      in_range(rc, 200, 300, line);

    send_socket(sock, "RCPT TO: " + to + "\r\n", deadline);
    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc); in_range(rc, 200, 300, line);

    send_socket(sock, "DATA\r\n", deadline);

    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc); in_range(rc, 200, 300, line);

    send_socket(sock, "Subject: " + subject + "\r\n", deadline);
    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc); in_range(rc, 200, 400, line);

    send_socket(sock, body + "\r\n.\r\n", deadline);
    line = read_line(sock, deadline); rc = rcode(line);
    no_hangup(rc); in_range(rc, 200, 300, line);

    send_socket(sock, "QUIT\r\n", deadline);

    read_line(sock, deadline);

    std::cout << "[smtp] message delivered." << std::endl;
    }
  catch (DeadlineException& de)
    {
    throw SMTPTooSlowException();
    }
  }
