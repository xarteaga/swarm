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

#include "shared.h"
#include "config.h"
#include <cstring>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>

class shared_memory_impl : public swarm::shared::memory
{
private:
  const std::string filename;
  const std::string semaphore_req_filename;
  const std::string semaphore_rep_filename;
  std::size_t       size;
  bool              unlink_after_use;
  int               fd      = 0;
  void*             sh_ptr  = nullptr;
  sem_t*            sem_req = nullptr;
  sem_t*            sem_rep = nullptr;

public:
  shared_memory_impl(const std::string& filename_, std::size_t size_, bool unlink_after_use_) :
    filename(filename_),
    semaphore_req_filename(filename_ + ".req"),
    semaphore_rep_filename(filename_ + ".rep"),
    size(size_),
    unlink_after_use(unlink_after_use_)
  {
    fd = shm_open(filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    SWARM_ASSERT(fd >= 0, "Error opening shared memory with file name '%s': %s", filename.c_str(), strerror(errno));

    SWARM_ASSERT(ftruncate(fd, size) >= 0, "Error running truncate: %s", strerror(errno));

    sh_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    SWARM_ASSERT(sh_ptr != MAP_FAILED and sh_ptr != nullptr, "Error mapping shared memory: %s", strerror(errno));

    // Open request semaphore
    sem_req = sem_open(semaphore_req_filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);
    SWARM_ASSERT(sem_req != SEM_FAILED and sem_req != nullptr, "Error mapping request semaphore: %s", strerror(errno));

    // Flush semaphore
    if (unlink_after_use) {
      struct timespec ts = {};
      while (sem_timedwait(sem_req, &ts) == 0) {
        ; // Do nothing
      }
    }

    // Open reply semaphore
    sem_rep = sem_open(semaphore_rep_filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);
    SWARM_ASSERT(sem_rep != SEM_FAILED and sem_rep != nullptr, "Error mapping reply semaphore: %s", strerror(errno));

    // Flush semaphore
    if (unlink_after_use) {
      struct timespec ts = {};
      while (sem_timedwait(sem_rep, &ts) == 0) {
        ; // Do nothing
      }
    }
  }

  ~shared_memory_impl()
  {
    if (fd >= 0) {
      memset(sh_ptr, 0, size);
      munmap(sh_ptr, size);
      if (unlink_after_use) {
        shm_unlink(filename.c_str());
      }
      close(fd);
    }

    if (sem_req) {
      if (unlink_after_use) {
        sem_unlink(semaphore_req_filename.c_str());
      }
      sem_close(sem_req);
    }
    if (sem_rep) {
      if (unlink_after_use) {
        sem_unlink(semaphore_rep_filename.c_str());
      }
      sem_close(sem_rep);
    }
  }

  bool available() override
  {
    struct timespec ts = {};
    SWARM_ASSERT(clock_gettime(CLOCK_REALTIME, &ts) >= 0, "Error getting getting time");

    ts.tv_sec += 1;

    int ret = sem_timedwait(sem_req, &ts);
    SWARM_ASSERT((ret == -1 and errno == ETIMEDOUT) or ret == 0,
                 "Unexpected semaphore wait return value (%d): %s",
                 ret,
                 strerror(errno));

#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
    if (ret == 0) {
      printf(" -- Request received\n");
    }
#endif /* SWARM_ENABLE_DEBUG_TRACE */

    return (ret == 0);
  }

  void write(const void* data) override
  {
    // Actual copy
    memcpy(sh_ptr, data, size);

#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
    printf(" -- Data written: %s\n", (const char*)data);
#endif /* SWARM_ENABLE_DEBUG_TRACE */

    // Send reply signal
    sem_post(sem_rep);

#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
    printf(" -- Reply posted\n");
#endif /* SWARM_ENABLE_DEBUG_TRACE */
  }

  void send_request() override {}

  bool read(void* data) override
  {
    // Send request signal
    sem_post(sem_req);

#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
    printf(" -- Request posted\n");
#endif /* SWARM_ENABLE_DEBUG_TRACE */

    struct timespec ts = {};
    SWARM_ASSERT(clock_gettime(CLOCK_REALTIME, &ts) >= 0, "Error getting getting time");

    // Set timeout
    ts.tv_nsec += 1UL /* ms */ * 1000UL /* to us */ * 1000UL /* to ns*/;
    while (ts.tv_nsec > 1000UL * 1000UL * 1000UL) {
      ts.tv_nsec -= 1000UL * 1000UL * 1000UL;
      ts.tv_sec++;
    }

    int ret = sem_timedwait(sem_rep, &ts);
    SWARM_ASSERT(errno == ETIMEDOUT or ret == 0, "Unexpected semaphore wait return value (%d)", ret);

    if (ret == 0) {
#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
      printf(" -- Reply received\n");
#endif /* SWARM_ENABLE_DEBUG_TRACE */

      memcpy(data, sh_ptr, size);
#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
      printf(" -- Data read: %s\n", (const char*)data);
#endif /* SWARM_ENABLE_DEBUG_TRACE */
    } else {
#if SWARM_SWARM_ENABLE_DEBUG_TRACE_TRACE
      printf(" -- No reply\n");
#endif /* SWARM_ENABLE_DEBUG_TRACE */
    }

    return (ret == 0);
  }
};

swarm::shared::memory::ptr
swarm::shared::memory::make(const std::string& filename, std::size_t size, bool unlink_after_use)
{
  return std::make_shared<shared_memory_impl>(filename, size, unlink_after_use);
}