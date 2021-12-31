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

#include "args.h"
#include "config.h"
#include "hostnames.h"
#include "ssh.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <unistd.h>

static std::atomic<bool> quit = {false};

static void sig_handler(int signo)
{
  quit = true;
}

static void print_help(const char* prog)
{
  printf("Usage: %s [options]\n", prog);
  printf("-n        Number of repetitions (infinite by default)\n");
  printf("-i        Interval in seconds (1 second)\n");
  printf("-h,--help This message\n");
}

int main(int argc, char** argv)
{
  // Signal handlers
  SWARM_ASSERT(signal(SIGINT, sig_handler) == SIG_DFL, "Error, the system cannot catch SIGINT");
  SWARM_ASSERT(signal(SIGABRT, sig_handler) == SIG_DFL, "Error, the system cannot catch SIGINT");
  SWARM_ASSERT(signal(SIGALRM, sig_handler) == SIG_DFL, "Error, the system cannot catch SIGINT");

  // Parse arguments
  swarm::args args(argc, argv);

  // Parse help
  {
    std::string help_str = args.get_first_param_match("^((\\-){1,2}((h)|(help)))$", 0);
    if (not help_str.empty()) {
      print_help(argv[0]);
      return 0;
    }
  }

  // Parse number of repetitions
  std::size_t n = 0;
  {
    std::string n_str = args.get_first_param_match("\\-n", 1);

    // Check if -n argument is present
    if (not n_str.empty()) {
      n = std::atoi(n_str.c_str());
    }
  }

  // Parse interval in seconds
  std::size_t interval_us = 1000000UL;
  {
    std::string i_str = args.get_first_param_match("\\-i", 1);

    // Check if -n argument is present
    if (not i_str.empty()) {
      interval_us = static_cast<std::size_t>(std::strtod(i_str.c_str(), nullptr) * 1e6);
    }
  }

  // Retrieve available hostnames
  std::vector<std::string> hostnames = swarm::hostname::get_all();

  // Create session for each hostname
  std::vector<swarm::ssh::session_ptr> sessions;
  sessions.reserve(hostnames.size());
  for (const std::string& hostname : hostnames) {
    sessions.emplace_back(swarm::ssh::make_session(hostname));
  }

  // Counter for table header print
  std::size_t head_count = 0;

  // Forever loop unless signal is handled or maximum loops is reached
  while (not quit) {
    // Get the current of the beginning
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    // Print table header
    if (head_count == 0) {
      printf("+----------------------+------------+------------+\n");
      printf("| %20s | %10s | %10s | %10s |\n", "Hostname", "Lat. [ms]", "CPU [%]", "Fitness");
      printf("+----------------------+------------+------------+\n");
    }
    head_count = (head_count + 1) % 10;

    // For each session...
    for (swarm::ssh::session_ptr session : sessions) {
      const double measure_time_s = 0.05;

      // Calculate parameters
      int    latency_ms  = 0;
      int    cpu_percent = 0;
      double fitness     = session->fitness(measure_time_s, &cpu_percent, &latency_ms);

      // Print latency
      printf("| %20s | %10d | %10d | %10.2f |\n", session->get_hostname().c_str(), latency_ms, cpu_percent, fitness);
    }

    // If n is set, then the loop is finite
    if (n != 0) {
      // Decrease count
      n--;

      // If the n reached 0, then quit
      quit = (n == 0);
    }

    // Sleep to match interval
    if (not quit and interval_us != 0) {
      // Get time of the end
      std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

      // Calculate elapsed time
      std::size_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

      // Sleep to match the interval time
      if (elapsed_us < interval_us)
        usleep(interval_us - elapsed_us);
    }
  }

  // Quit time!
  return 0;
}