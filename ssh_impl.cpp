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

#include "config.h"
#include "ssh.h"
#include "string_helpers.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <libssh/libssh.h>
#include <vector>

namespace swarm {
namespace ssh {

class channel_impl : public channel
{
private:
  ssh_channel channel = nullptr;

public:
  explicit channel_impl(ssh_session& session)
  {
    channel = ssh_channel_new(session);
    SWARM_ASSERT(channel != nullptr, "Error creating new channel");
  }

  ~channel_impl()
  {
    if (channel == nullptr) {
      return;
    }

    if (ssh_channel_is_open(channel)) {
      ssh_channel_send_eof(channel);

      ssh_channel_close(channel);
    }

    ssh_channel_free(channel);

    channel = nullptr;
  }

  int execute(const std::string& command) override
  {
    char buffer[256];

    SWARM_ASSERT(ssh_channel_open_session(channel) == SSH_OK, "Error opening SSH channel");

    SWARM_ASSERT(ssh_channel_request_exec(channel, command.c_str()) == SSH_OK, "Error opening SSH session");
    ssh_channel_send_eof(channel);

    while (not ssh_channel_is_eof(channel)) {
      // Read stdout
      int nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
      if (nbytes > 0) {
        int n = write(1, buffer, nbytes) != (unsigned int)nbytes;
        SWARM_ASSERT(n != nbytes, "Error writing in stdout");
        continue;
      }

      // Read stderr
      nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1);
      if (nbytes > 0) {
        int n = write(2, buffer, nbytes) != (unsigned int)nbytes;
        SWARM_ASSERT(n != nbytes, "Error writing in stderr");
        continue;
      }

      break;
    }

    return ssh_channel_get_exit_status(channel);
  }
  std::string execute_to_str(const std::string& command)
  {
    char        buffer[256];
    std::string ret;

    SWARM_ASSERT(ssh_channel_open_session(channel) == SSH_OK, "Error opening SSH channel");

    SWARM_ASSERT(ssh_channel_request_exec(channel, command.c_str()) == SSH_OK, "Error opening SSH session");
    ssh_channel_send_eof(channel);

    while (not ssh_channel_is_eof(channel)) {
      // Read stdout
      int nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
      if (nbytes > 0) {
        ret += std::string(buffer);
        continue;
      }

      // Read stderr
      nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1);
      if (nbytes > 0) {
        ret += std::string(buffer);
        continue;
      }

      break;
    }

    // Ignore return status
    //    int status = ssh_channel_get_exit_status(channel);
    //    SWARM_ASSERT(status == 0, "Error. Remote execution finished with exit code %d", status);

    ssh_channel_close(channel);

    return ret;
  }

  int top(double measure_time_s) override
  {
    std::string vmstat_str =
        execute_to_str("stat_cpu() { grep \"cpu \" /proc/stat | grep -o -m 1 \"[0-9]*\" | head -n 1; } ;S=" +
                       std::to_string(measure_time_s) +
                       ";C1=$(stat_cpu); sleep $S;C2=$(stat_cpu);N=$(grep \"processor\" /proc/cpuinfo | wc -l);echo "
                       "\\(\\(100*\\($C2-$C1\\)\\)/\\($S*$N\\)\\)/100 | bc");

    return std::min(static_cast<int>(std::stof(vmstat_str)), 100);
  }
};
class sftp_write_impl : public sftp_write
{
private:
  ssh_session& session;
  ssh_scp      scp = nullptr;

public:
  sftp_write_impl(ssh_session& session_, const std::string& location) : session(session_)
  {
    scp = ssh_scp_new(session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, location.c_str());
    SWARM_ASSERT(scp != nullptr, "Error allocating scp session: %s\n", ssh_get_error(session));
    SWARM_ASSERT(ssh_scp_init(scp) == SSH_OK, "Error initializing scp writer session: %s\n", ssh_get_error(session));
  }

  void push_directory(const std::string& path) override
  {
    std::vector<std::string> dir_list = string_helpers::split(path, '/');
    for (const std::string& dir : dir_list) {
      for (int trial = 0; trial < SWARM_MAX_NOF_TRIALS; trial++) {
        // Try to create directory
        int err = ssh_scp_push_directory(scp, dir.c_str(), S_IRWXU);

        // Next trial
        if (err != SSH_OK and ssh_get_error_code(session) == 1) {
          usleep(1000);
          continue;
        }

        // Otherwise, fatal error
        SWARM_ASSERT(err == SSH_OK, "Can't create remote directory: %s\n", ssh_get_error(session));

        break;
      }
    }
  }

  void push_file(const std::string& filename, const std::size_t& size) override
  {
    SWARM_ASSERT(ssh_scp_push_file64(scp, filename.c_str(), size, S_IRUSR | S_IWUSR) == SSH_OK,
                 "Can't create remote file: %s\n",
                 ssh_get_error(session));
  }

  void write(const char* buffer, std::size_t nbytes) override
  {
    if (nbytes == 0) {
      return;
    }

    SWARM_ASSERT(
        ssh_scp_write(scp, buffer, nbytes) == SSH_OK, "Can't write to remote file: %s\n", ssh_get_error(session));
  }

  ~sftp_write_impl()
  {
    if (scp == nullptr) {
      return;
    }

    ssh_blocking_flush(session, 10);

    ssh_scp_close(scp);
    ssh_scp_free(scp);
  }
};

class sftp_read_impl : public sftp_read
{
private:
  ssh_session& session;
  ssh_scp      scp = nullptr;

public:
  sftp_read_impl(ssh_session& session_, const std::string& location) : session(session_)
  {
    scp = ssh_scp_new(session, SSH_SCP_READ, location.c_str());
    SWARM_ASSERT(scp != nullptr, "Error allocating scp session: %s\n", ssh_get_error(session));
    SWARM_ASSERT(ssh_scp_init(scp) == SSH_OK, "Error initializing scp reader session: %s\n", ssh_get_error(session));

    SWARM_ASSERT(ssh_scp_pull_request(scp) == SSH_SCP_REQUEST_NEWFILE,
                 "Error receiving information about file: %s",
                 ssh_get_error(session));
  }

  bool is_eof() override { return ssh_scp_pull_request(scp) == SSH_SCP_REQUEST_EOF; }

  std::size_t read(void* buffer, std::size_t nbytes) override
  {
    ssh_scp_accept_request(scp);

    int ret = ssh_scp_read(scp, buffer, nbytes);

    SWARM_ASSERT(ret != SSH_ERROR, "Error receiving file data: %s\n", ssh_get_error(session));

    return ret;
  }

  ~sftp_read_impl()
  {
    if (scp == nullptr) {
      return;
    }

    ssh_scp_close(scp);
    ssh_scp_free(scp);
  }
};

static int top_impl(ssh_session s, double measure_time_s)
{
  if (not ssh_is_connected(s)) {
    return -1;
  }

  return channel_impl(s).top(measure_time_s);
}

class session_impl : public session
{
private:
  ssh_session session = nullptr;
  std::string hostname;

  static int verify_knownhost(ssh_session session)
  {
    enum ssh_known_hosts_e state;
    unsigned char*         hash       = nullptr;
    ssh_key                srv_pubkey = nullptr;
    size_t                 hlen;
    char                   buf[10];
    char*                  hexa;
    char*                  p;
    int                    cmp;
    int                    rc;
    rc = ssh_get_server_publickey(session, &srv_pubkey);
    if (rc < 0) {
      return -1;
    }
    rc = ssh_get_publickey_hash(srv_pubkey, SSH_PUBLICKEY_HASH_SHA1, &hash, &hlen);
    ssh_key_free(srv_pubkey);
    if (rc < 0) {
      return -1;
    }
    state = ssh_session_is_known_server(session);
    switch (state) {
      case SSH_KNOWN_HOSTS_OK:
        /* OK */
        break;
      case SSH_KNOWN_HOSTS_CHANGED:
        fprintf(stderr, "Host key for server changed: it is now:\n");
        ssh_print_hexa("Public key hash", hash, hlen);
        fprintf(stderr, "For security reasons, connection will be stopped\n");
        ssh_clean_pubkey_hash(&hash);
        return -1;
      case SSH_KNOWN_HOSTS_OTHER:
        fprintf(stderr,
                "The host key for this server was not found but an other"
                "type of key exists.\n");
        fprintf(stderr,
                "An attacker might change the default server key to"
                "confuse your client into thinking the key does not exist\n");
        ssh_clean_pubkey_hash(&hash);
        return -1;
      case SSH_KNOWN_HOSTS_NOT_FOUND:
        fprintf(stderr, "Could not find known host file.\n");
        fprintf(stderr,
                "If you accept the host key here, the file will be"
                "automatically created.\n");
        /* FALL THROUGH to SSH_SERVER_NOT_KNOWN behavior */
      case SSH_KNOWN_HOSTS_UNKNOWN:
        hexa = ssh_get_hexa(hash, hlen);
        fprintf(stderr, "The server is unknown. Do you trust the host key?\n");
        fprintf(stderr, "Public key hash: %s\n", hexa);
        ssh_string_free_char(hexa);
        ssh_clean_pubkey_hash(&hash);
        p = fgets(buf, sizeof(buf), stdin);
        if (p == nullptr) {
          return -1;
        }
        cmp = strncasecmp(buf, "yes", 3);
        if (cmp != 0) {
          return -1;
        }
        rc = ssh_session_update_known_hosts(session);
        if (rc < 0) {
          fprintf(stderr, "Error %s\n", strerror(errno));
          return -1;
        }
        break;
      case SSH_KNOWN_HOSTS_ERROR:
        fprintf(stderr, "Error %s", ssh_get_error(session));
        ssh_clean_pubkey_hash(&hash);
        return -1;
    }
    ssh_clean_pubkey_hash(&hash);
    return 0;
  }

public:
  explicit session_impl(const std::string& hostname_) : hostname(hostname_)
  {
    // Create session
    session = ssh_new();

    // Skip if session was not possible to open
    SWARM_ASSERT(session != nullptr, "Error creating new SSH session");

    SWARM_ASSERT(ssh_options_set(session, SSH_OPTIONS_HOST, hostname.c_str()) == SSH_OK,
                 "Error setting the SSH hostname");
    //    int nodelay = 1;
    ////    SWARM_ASSERT(ssh_options_set(session, SSH_OPTIONS_NODELAY, &nodelay) == SSH_OK, "Error setting no delay");
    ////    SWARM_ASSERT(ssh_options_set(session, SSH_OPTIONS_COMPRESSION, "yes") == SSH_OK, "Error setting
    /// compression");
    ////
    ////    int timeout_s  = 0;
    ////    int timeout_us = 1000;
    ////    SWARM_ASSERT(ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout_s) == SSH_OK, "Error setting timeout");
    ////    SWARM_ASSERT(ssh_options_set(session, SSH_OPTIONS_TIMEOUT_USEC, &timeout_us) == SSH_OK,
    ////                 "Error setting timeout in us");

    // Connect to server
    SWARM_ASSERT(ssh_connect(session) == SSH_OK, "Error connection to hostname '%s'", hostname.c_str());

    // Verify known host
    SWARM_ASSERT(verify_knownhost(session) >= 0, "Failed to verify known host");

    // Authenticate user
    SWARM_ASSERT(ssh_userauth_publickey_auto(session, nullptr, nullptr) == SSH_AUTH_SUCCESS,
                 "Authentication failed: %s\n",
                 ssh_get_error(session));
  }

  explicit session_impl(const std::vector<std::string>& hostnames)
  {
    std::vector<ssh_session> sessions(hostnames.size());
    int                      best_percent_cpu = 200;

    // For each hostname create session
    for (std::size_t i = 0; i < hostnames.size(); i++) {
      // Create session
      sessions[i] = ssh_new();

      // Skip if session was not possible to open
      if (sessions[i] == nullptr) {
        continue;
      }

      ssh_options_set(sessions[i], SSH_OPTIONS_HOST, hostnames[i].c_str());

      // Connect to server
      for (std::size_t trial = 0; trial < SWARM_MAX_NOF_TRIALS; trial++) {
        if (ssh_connect(sessions[i]) == SSH_OK) {
          break;
        }
        //        usleep(1000);
      }

      if (not ssh_is_connected(sessions[i])) {
        ssh_free(sessions[i]);
        sessions[i] = nullptr;
        continue;
      }

      // Verify the server's identity
      SWARM_ASSERT(verify_knownhost(sessions[i]) >= 0, "Failed to verify known host");

      SWARM_ASSERT(ssh_userauth_publickey_auto(sessions[i], nullptr, nullptr) == SSH_AUTH_SUCCESS,
                   "Authentication failed: %s\n",
                   ssh_get_error(sessions[i]));

      // Get CPU percent
      int cpu_percent = top_impl(sessions[i], 0.01);

      // Skip if the CPU load is invalid
      if (cpu_percent < 0) {
        continue;
      }

      // Compare with the previous best
      if (cpu_percent < best_percent_cpu) {
        best_percent_cpu = cpu_percent;
        session          = sessions[i];
        hostname         = hostnames[i];
      }
    }

    SWARM_ASSERT(best_percent_cpu != 200, "Error connection to host");

    // For each created SSH session close and free the ones that do not match the best
    for (std::size_t i = 0; i < hostnames.size(); i++) {
      // Skip invalid
      if (sessions[i] == nullptr) {
        continue;
      }

      // Skip matches
      if (sessions[i] == session) {
        continue;
      }

      // Close
      if (ssh_is_connected(sessions[i])) {
        ssh_disconnect(sessions[i]);
      }

      ssh_free(sessions[i]);
    }
  }

  ~session_impl()
  {
    if (session == nullptr) {
      return;
    }

    if (ssh_is_connected(session)) {
      ssh_disconnect(session);
    }

    ssh_free(session);

    session = nullptr;
  }

  std::string get_hostname() const override { return hostname; }

  channel_ptr    make_channel() override { return std::make_shared<channel_impl>(session); }
  sftp_write_ptr make_sftp_write(const std::string& location) override
  {
    return std::make_shared<sftp_write_impl>(session, location);
  }
  sftp_read_ptr make_sftp_read(const std::string& location) override
  {
    return std::make_shared<sftp_read_impl>(session, location);
  }

  void sftp_copy_local_to_remote(const std::string& local_path, const std::string& remote_path) override
  {
    swarm::ssh::sftp_write_ptr sftp = make_sftp_write("/");

    // Create directory in remote host
    std::size_t pos = remote_path.find_last_of('/');
    if (pos != remote_path.npos) {
      sftp->push_directory(remote_path.substr(0, pos));
    }

    // Open local file
    std::ifstream file;
    file.open(local_path, std::ifstream::ate | std::ifstream::binary);

    // Get file size
    std::size_t remainder = file.tellg();
    file.seekg(0);

    // Create file in remote host
    sftp->push_file(remote_path, remainder);

    // Create buffer
    std::array<char, SWARM_SCP_BUFFER_SZ> buffer = {};

    // Copy pipe output into the destination file
    while (remainder > 0) {
      // Read
      std::size_t n = file.readsome(buffer.data(), buffer.size());

      // Write remote file
      sftp->write(buffer.data(), n);

      // Subtract number of bytes
      remainder -= n;
    }

    file.close();
  }

  void sftp_copy_remote_to_local(const std::string& remote_path, const std::string& local_path) override
  {
    swarm::ssh::sftp_read_ptr                sftp   = make_sftp_read(remote_path);
    std::array<uint8_t, SWARM_SCP_BUFFER_SZ> buffer = {};
    std::ofstream                            local_file;

    local_file.open(local_path);

    while (not sftp->is_eof()) {
      std::size_t n = sftp->read(buffer.data(), buffer.size());
      local_file.write((char*)buffer.data(), (uint32_t)n);
    }

    local_file.close();
  }

  int top(double measure_time_s) override { return top_impl(session, measure_time_s); }
};

session_ptr make_session(const std::string& hostname)
{
  return std::make_shared<session_impl>(hostname);
}

session_ptr make_session(const std::vector<std::string>& hostnames)
{
  // If there is only one hostname, make the session with it
  if (hostnames.size() == 1) {
    return make_session(hostnames[0]);
  }

  // Otherwise select the one with lowest CPU
  return std::make_shared<session_impl>(hostnames);
}

} // namespace ssh
} // namespace swarm