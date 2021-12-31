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

#include "hostnames.h"
#include "config.h"
#include "shared.h"
#include "string_helpers.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>

swarm::hostname::vector_t swarm::hostname::get_all()
{
  swarm::hostname::vector_t list;

  // Get the hostname list in C string
  const char* hostnames_c = getenv(SWARM_ENV_VAR_HOSTNAME_LIST);
  if (hostnames_c == nullptr) {
    hostnames_c = SWARM_DEFAULT_HOSTNAME_LIST;
  }

  // Convert to C++ string
  std::string hostnames = std::string(hostnames_c);

  // Split C++ string
  return string_helpers::split(hostnames, SWARM_HOSTNAME_LIST_DELIMITER);
}

std::string swarm::hostname::get_local()
{
  // Get the hostname in C string
  char hostname_c[256] = {};
  SWARM_ASSERT(gethostname(hostname_c, sizeof(hostname_c)) == 0, "Error getting hostname: %s", strerror(errno));

  // Convert to C++ string
  return hostname_c;
}

std::string swarm::hostname::get_lb()
{
  swarm::shared::request<char[SWARM_HOSTNAME_MAX_LENGTH]> request(SWARM_HOSTNAME_IPC_FILENAME);

  char hostname[SWARM_HOSTNAME_MAX_LENGTH] = {};

  request.send_request();

  // Read hostname from the load balancer
  request.read(hostname);

  // If the load balance cannot be read, then return an empty string
  return hostname;
}
