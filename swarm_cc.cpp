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
#include <cstring>
#include <iostream>
#include <set>
#include <thread>

static std::set<std::string> supported_languages = {"c", "c++"};
static std::set<std::string> excluded_targets    = {"/dev/null"};

static void precompile(std::string precompile_command)
{
  int status = system(precompile_command.c_str());
  SWARM_ASSERT(status == SWARM_PRECOMPILER_EXPECTED_STATUS,
               "Error. Precompiler exited with status code %d and expected %d",
               status,
               SWARM_PRECOMPILER_EXPECTED_STATUS);
}

static int bypass_swarm_cc(const swarm::args& args)
{
  std::string cmd = args.get_command();

  fprintf(stderr, "-- Bypassing swarm-cc command -- %s\n", cmd.c_str());

  return system(cmd.c_str());
}

int main(int argc, char** argv)
{
  // Parse input parameters
  swarm::args args(argc, argv);

  // Delete gcc-10 unsupported parameters...
  args.delete_args("ftrivial", 1);

  // Get source file, extensions ".c", ".cpp" and ".cc"
  std::string source_file = args.get_first_param_match("(\\.c$)|(\\.cpp$)|(\\.cc$)");

  // Get compile target
  std::string local_compile_target = args.get_first_param_match("\\.o$");

  // If no source nor typical C/C++ compiled file extension file is found, bypass swarm-cc
  if (source_file.empty() or local_compile_target.empty()) {
    return bypass_swarm_cc(args);
  }
  fprintf(stderr, "-- Processing swarm-cc command -- %s\n", args.get_command().c_str());

  //  return bypass_swarm_cc(args);

  // Generate local precompile target name
  std::string local_precompile_target = SWARM_REMOTE_PATH + "/" + source_file;

  // Create local precompilation directory
  std::string local_mkdir_command =
      "mkdir -p " + local_precompile_target.substr(0, local_precompile_target.find_last_of("/"));
  SWARM_ASSERT(system(local_mkdir_command.c_str()) == 0, "Error creatimg folder");

  // Remote base path
  // TODO: Add some unique path from this hostname
  std::string remote_path_base = SWARM_REMOTE_PATH + swarm::hostname::get_local() + "/";

  // Generate remote file name
  std::string remote_compile_target = remote_path_base + local_compile_target;

  // Generate remote precompiled file name
  std::string remote_precompile_target = remote_path_base + source_file;

  // Copy original compiler arguments to generate precompiler command
  swarm::args precompile_args = args;

  // Substitute
  precompile_args.substitute_all_param_match("\\.o$", local_precompile_target);
  precompile_args.append("-E");

  // Copy original compiler arguments to generate compilation command
  swarm::args compile_args = args;

  // Remove precompiler parameters with secondary parameters
  compile_args.delete_args("(\\-MT)|(\\-MF)|(\\-include)|(\\-I$)", 2);

  // Remove precompiler flags
  compile_args.delete_args("(\\-D)|(\\-I)|(\\-M)", 1);

  std::string local_target = compile_args.get_first_param_match("\\.o$");
  compile_args.substitute_all_param_match("\\.o$", remote_compile_target);
  compile_args.substitute_all_param_match("(\\.c$)|(\\.cpp$)|(\\.cc$)", remote_precompile_target);
  std::string local_source = compile_args.get_last_param();

  //    printf("Precompile command:\n\t%s\n", precompile_command.c_str());
  //    printf("Compile command:\n\t%s\n", compile_command.c_str());
  //  fprintf(stderr, "Original command:\n\t%s\n", args.get_command().c_str());
  //  fprintf(stderr, "Precompile command:\n\t%s\n", precompile_args.get_command().c_str());
  //  fprintf(stderr, "Compile command:\n\t%s\n", compile_args.get_command().c_str());

  // Precompile
  std::thread precompile_thread(precompile, precompile_args.get_command());

  // Get hostnames candidates
  std::vector<std::string> hostnames = swarm::hostname::get_all();

  // Create SSH session
  swarm::ssh::session_ptr session = swarm::ssh::make_session(hostnames);

  precompile_thread.join();

  // Call precompiler and write file in remote machine
  session->sftp_copy_local_to_remote(local_precompile_target, remote_precompile_target);

  // Execute compilation command in remote machine
  int status = session->make_channel()->execute(compile_args.get_command());
  if (status != 0) {
    return status;
  }

  // Copy remote file to local
  session->sftp_copy_remote_to_local(remote_compile_target, local_compile_target);

  return status;
}