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

#ifndef SWARM__HOSTNAMES_H_
#define SWARM__HOSTNAMES_H_

#include "config.h"
#include <string>
#include <vector>

namespace swarm {
namespace hostname {

typedef std::vector<std::string> vector_t;

vector_t    get_all();
std::string get_local();
std::string get_lb();

} // namespace hostname
} // namespace swarm

#endif // SWARM__HOSTNAMES_H_
