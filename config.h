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

#ifndef SWARM__CONFIG_H_
#define SWARM__CONFIG_H_

#define SWARM_API __attribute__((__visibility__("default")))

#define SWARM_ENV_VAR_HOSTNAME_LIST "SWARM_HOSTNAMES"
#define SWARM_DEFAULT_HOSTNAME_LIST "localhost"
#define SWARM_HOSTNAME_LIST_DELIMITER ','

#define SWARM_ENV_VAR_HOSTNAME "HOSTNAME"
#define SWARM_DEFAULT_HOSTNAME "no-host-name"

#define SWARM_REMOTE_PATH std::string("/tmp/swarm/")
#define SWARM_SCP_BUFFER_SZ (1024 * 1024)
#define SWARM_MAX_NOF_TRIALS 2
#define SWARM_PRECOMPILER_EXPECTED_STATUS 0

#define SWARM_ASSERT(CONDITION, FMT, ...)                                                                              \
  do {                                                                                                                 \
    if (not(CONDITION)) {                                                                                              \
      fprintf(stderr, FMT "\n", ##__VA_ARGS__);                                                                        \
      std::exit(-1);                                                                                                   \
    }                                                                                                                  \
  } while (false)

#endif // SWARM__CONFIG_H_
