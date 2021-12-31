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
#include "shared.h"
#include "ssh.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <semaphore.h>
#include <thread>
#include <unistd.h>
#include <vector>

static std::atomic<bool>   quit         = {false};
static std::vector<double> host_fitness = {};
static std::size_t         interval_us  = 0; // 0 for free-running

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

static void update_cpu_loads(std::vector<swarm::ssh::session_ptr> sessions)
{
  // For each session get CPU percent and update value
  for (std::size_t i = 0; i < sessions.size(); i++) {
    // Select session
    swarm::ssh::session_ptr& session = sessions[i];

    // Get host fitness
    double measure_time_s = 0.01;
    int    latency_ms     = 0;
    int    cpu_percent    = 0;
    double fitness        = session->fitness(measure_time_s, &cpu_percent, &latency_ms);

    // Write shared variable
    host_fitness[i] = fitness;

    printf("-- %20s -- %10d %10d %10.2f\n", session->get_hostname().c_str(), cpu_percent, latency_ms, fitness);
  }
}

static void top_thread(std::vector<swarm::ssh::session_ptr> sessions)
{
  // Forever loop unless signal is handled
  while (not quit) {
    // Get the current of the beginning
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    update_cpu_loads(sessions);

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

  // Quit thread, do nothing
}

int main(int argc, char** argv)
{
  // Signal handlers
  SWARM_ASSERT(signal(SIGINT, sig_handler) == SIG_DFL, "Error, the system cannot catch SIGINT");
  SWARM_ASSERT(signal(SIGABRT, sig_handler) == SIG_DFL, "Error, the system cannot catch SIGINT");
  SWARM_ASSERT(signal(SIGALRM, sig_handler) == SIG_DFL, "Error, the system cannot catch SIGINT");

  // Parse arguments
  swarm::args args(argc, argv);

  // Retrieve available hostnames
  std::vector<std::string> hostnames = swarm::hostname::get_all();

  // Create a session for each hostname
  std::vector<swarm::ssh::session_ptr> sessions;
  sessions.reserve(hostnames.size());
  for (const std::string& hostname : hostnames) {
    sessions.emplace_back(swarm::ssh::make_session(hostname));
  }

  // Create CPU percent and initialise to -1
  host_fitness.resize(sessions.size());
  for (double& cpu_fitness_ : host_fitness) {
    cpu_fitness_ = 0.0;
  }

  // Create asynchronous thread
  std::thread thread(top_thread, sessions);

  // Create shared memory for hostname
  swarm::shared::reply<char[SWARM_HOSTNAME_MAX_LENGTH]> reply(SWARM_HOSTNAME_IPC_FILENAME);
  while (not quit) {
    // Skip processing if no request is available
    if (not reply.available()) {
      continue;
    }

    char hostname[SWARM_HOSTNAME_MAX_LENGTH] = "localhost";

    // Update CPU loads
    //    update_cpu_loads(sessions);

    // Find minimum
    double      best_host_fitness = 0;
    std::size_t best_host_idx     = 0;
    for (std::size_t i = 0; i < host_fitness.size(); i++) {
      double host_fitness_ = host_fitness[i];
      if (best_host_fitness < host_fitness_) {
        best_host_fitness = host_fitness_;
        best_host_idx     = i;
      }
    }

    // Convert C++ to C fix size type by copying
    strcpy(hostname, hostnames[best_host_idx].c_str());

    //    printf("-- %10s -- %20s: %10d\n", "reply", hostname, best_cpu_load);

    // Write hostname
    reply.write(hostname);
  }

  // Quit time!
  return 0;
}