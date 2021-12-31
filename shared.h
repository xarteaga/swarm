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

#ifndef SWARM_SHARED_H
#define SWARM_SHARED_H

#include <memory>

namespace swarm {

namespace shared {

class memory
{
public:
  virtual bool available()        = 0;
  virtual void write(const void*) = 0;

  virtual void send_request() = 0;
  virtual bool read(void*)    = 0;

  typedef std::shared_ptr<memory> ptr;

  static ptr make(const std::string& filename, std::size_t size, bool unlink_after_use);
};

template <class T>
class reply
{
private:
  memory::ptr m = nullptr;

public:
  reply(const std::string& filename) { m = memory::make(filename, sizeof(T), true); }
  bool         available() { return m->available(); }
  virtual void write(const T& data) { m->write(&data); }
};

template <class T>
class request
{
private:
  memory::ptr m = nullptr;

public:
  request(const std::string& filename) { m = memory::make(filename, sizeof(T), false); }

  void send_request() { m->send_request(); }
  bool read(T& data) { return m->read(&data); }
};

} // namespace shared

} // namespace swarm

#endif // SWARM_SHARED_H
