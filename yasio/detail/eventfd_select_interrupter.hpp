//////////////////////////////////////////////////////////////////////////////////////////
// A cross platform socket APIs, support ios & android & wp8 & window store universal app
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// detail/eventfd_select_interrupter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2008 Roelof Naude (roelof.naude at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef YASIO__EVENTFD_SELECT_INTERRUPTER_HPP
#define YASIO__EVENTFD_SELECT_INTERRUPTER_HPP

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
#  include <asm/unistd.h>
#else // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
#  include <sys/eventfd.h>
#endif // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8

#include <unistd.h>

namespace yasio
{
namespace inet
{

class eventfd_select_interrupter
{
public:
  // Constructor.
  inline eventfd_select_interrupter() { open_descriptors(); }

  // Destructor.
  inline ~eventfd_select_interrupter() { close_descriptors(); }

  // Recreate the interrupter's descriptors. Used after a fork.
  inline void recreate()
  {
    close_descriptors();

    write_descriptor_ = -1;
    read_descriptor_  = -1;

    open_descriptors();
  }

  // Interrupt the select call.
  inline void interrupt()
  {
    uint64_t counter(1UL);
    int result = ::write(write_descriptor_, &counter, sizeof(uint64_t));
    (void)result;
  }

  // Reset the select interrupt. Returns true if the call was interrupted.
  inline bool reset()
  {
    if (write_descriptor_ == read_descriptor_)
    {
      for (;;)
      {
        // Only perform one read. The kernel maintains an atomic counter.
        uint64_t counter(0);
        errno          = 0;
        int bytes_read = ::read(read_descriptor_, &counter, sizeof(uint64_t));
        if (bytes_read < 0 && errno == EINTR)
          continue;
        bool was_interrupted = (bytes_read > 0);
        return was_interrupted;
      }
    }
    else
    {
      for (;;)
      {
        // Clear all data from the pipe.
        char data[1024];
        int bytes_read = ::read(read_descriptor_, data, sizeof(data));
        if (bytes_read < 0 && errno == EINTR)
          continue;
        bool was_interrupted = (bytes_read > 0);
        while (bytes_read == sizeof(data))
          bytes_read = ::read(read_descriptor_, data, sizeof(data));
        return was_interrupted;
      }
    }
  }

  // Get the read descriptor to be passed to select.
  int read_descriptor() const { return read_descriptor_; }

private:
  // Open the descriptors. Throws on error.
  inline void open_descriptors()
  {
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
    write_descriptor_ = read_descriptor_ = syscall(__NR_eventfd, 0);
    if (read_descriptor_ != -1)
    {
      ::fcntl(read_descriptor_, F_SETFL, O_NONBLOCK);
      ::fcntl(read_descriptor_, F_SETFD, FD_CLOEXEC);
    }
#else // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
#  if defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
    write_descriptor_ = read_descriptor_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#  else  // defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
    errno             = EINVAL;
    write_descriptor_ = read_descriptor_ = -1;
#  endif // defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
    if (read_descriptor_ == -1 && errno == EINVAL)
    {
      write_descriptor_ = read_descriptor_ = ::eventfd(0, 0);
      if (read_descriptor_ != -1)
      {
        ::fcntl(read_descriptor_, F_SETFL, O_NONBLOCK);
        ::fcntl(read_descriptor_, F_SETFD, FD_CLOEXEC);
      }
    }
#endif   // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8

    if (read_descriptor_ == -1)
    {
      int pipe_fds[2];
      if (pipe(pipe_fds) == 0)
      {
        read_descriptor_ = pipe_fds[0];
        ::fcntl(read_descriptor_, F_SETFL, O_NONBLOCK);
        ::fcntl(read_descriptor_, F_SETFD, FD_CLOEXEC);
        write_descriptor_ = pipe_fds[1];
        ::fcntl(write_descriptor_, F_SETFL, O_NONBLOCK);
        ::fcntl(write_descriptor_, F_SETFD, FD_CLOEXEC);
      }
    }
  }

  // Close the descriptors.
  inline void close_descriptors()
  {
    if (write_descriptor_ != -1 && write_descriptor_ != read_descriptor_)
      ::close(write_descriptor_);
    if (read_descriptor_ != -1)
      ::close(read_descriptor_);
  }

  // The read end of a connection used to interrupt the select call. This file
  // descriptor is passed to select such that when it is time to stop, a single
  // 64bit value will be written on the other end of the connection and this
  // descriptor will become readable.
  int read_descriptor_;

  // The write end of a connection used to interrupt the select call. A single
  // 64bit non-zero value may be written to this to wake up the select which is
  // waiting for the other end to become readable. This descriptor will only
  // differ from the read descriptor when a pipe is used.
  int write_descriptor_;
};

} // namespace inet
} // namespace yasio

#endif // YASIO__EVENTFD_SELECT_INTERRUPTER_HPP
