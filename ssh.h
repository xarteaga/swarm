/**
 * This file is part of the swarm project
 *
 * Copyright (c) 2021 by Xavier Arteaga
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <memory>
#include <string>
#include <vector>

#ifndef SWARM_SSH
#define SWARM_SSH

namespace swarm {
namespace ssh {

class channel
{
public:
  virtual int execute(const std::string& command) = 0;
  virtual int top(double measure_time_s)          = 0;
};

typedef std::shared_ptr<channel> channel_ptr;

class sftp_write
{
public:
  virtual void push_directory(const std::string& path) = 0;

  virtual void push_file(const std::string& filename, const std::size_t& size) = 0;

  virtual void write(const char* buffer, std::size_t nbytes) = 0;
};

typedef std::shared_ptr<sftp_write> sftp_write_ptr;

class sftp_read
{
public:
  virtual bool        is_eof()                               = 0;
  virtual std::size_t read(void* buffer, std::size_t nbytes) = 0;
};

typedef std::shared_ptr<sftp_read> sftp_read_ptr;

class session
{
public:
  virtual std::string    get_hostname() const                                                                     = 0;
  virtual channel_ptr    make_channel()                                                                           = 0;
  virtual sftp_write_ptr make_sftp_write(const std::string& location)                                             = 0;
  virtual sftp_read_ptr  make_sftp_read(const std::string& location)                                              = 0;
  virtual void           sftp_copy_local_to_remote(const std::string& local_path, const std::string& remote_path) = 0;
  virtual void           sftp_copy_remote_to_local(const std::string& remote_path, const std::string& local_path) = 0;
  virtual int            top(double measure_time_s)                                                               = 0;
  virtual double         fitness(double measure_time_s, int* cpu_percent, int* latency_ms)                        = 0;
};

typedef std::shared_ptr<session> session_ptr;

SWARM_API session_ptr make_session(const std::string& hostname);

SWARM_API session_ptr make_session(const std::vector<std::string>& hostnames);

} // namespace ssh
} // namespace swarm

#endif // SWARM_SSH
