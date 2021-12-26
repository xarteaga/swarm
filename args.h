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

#ifndef SWARM_ARGS_H
#define SWARM_ARGS_H

#include "config.h"
#include <regex>
#include <string>
#include <vector>

namespace swarm {
class args
{
private:
  std::vector<std::string> list = {};

public:
  // Argument parse constructor
  args(int argc, char** argv)
  {
    std::regex regex_double_quote("(\".*\")");
    std::regex regex_single_quote("(\'.*\')");
    SWARM_ASSERT(argc > 1, "Invalid number of arguments (%d)", argc);

    // Allocate arguments
    list.resize(argc - 1);

    // For each param...
    for (std::size_t i = 1; i < (std::size_t)argc; i++) {
      // Check argument pointer is valid
      SWARM_ASSERT(argv[i] != nullptr, "Argument %d is invalid", (int)i);

      // Convert argument to C++ string
      std::string str = std::string(argv[i]);

      // Find single quoted text
      std::smatch m;
      if (std::regex_search(str, m, regex_single_quote)) {
        std::size_t open_quote_pos = str.find_first_of('\'');
        str.replace(open_quote_pos, 1, "\"\'");

        std::size_t close_quote_pos = str.find_last_of('\'');
        str.replace(close_quote_pos, 1, "\'\"");
      }

      // Find double quoted text
      if (std::regex_search(str, m, regex_double_quote)) {
        std::size_t open_quote_pos = str.find_first_of('\"');
        str.replace(open_quote_pos, 1, "\'\"");

        std::size_t close_quote_pos = str.find_last_of('\"');
        str.replace(close_quote_pos, 1, "\"\'");
      }

      // Put string in vector
      list[i - 1] = str;
    }
  }

  // Copy constructor
  args(const args& other) { list = other.list; }

  // Get command
  std::string get_command() const
  {
    std::string ret;
    for (const std::string& e : list) {
      ret += e + " ";
    }
    return ret;
  }

  void delete_args(const std::string& regexp, std::size_t count = 1)
  {
    std::regex  regex(regexp);
    std::cmatch m;

    // For each argument
    for (auto it = list.begin(); it != list.end();) {
      // Try to match regular expression with argument
      if (not std::regex_search(it->c_str(), m, regex)) {
        it++;
        continue;
      }

      // Delete the matched string and the following count elements
      for (std::size_t i = 0; i < count and it != list.end(); i++) {
        list.erase(it);
      }
    }
  }

  std::string get_first_param_match(const std::string& regexp, std::size_t offset = 0) const
  {
    std::regex  regex(regexp);
    std::cmatch m;

    // For each argument
    for (auto it = list.begin(); it != list.end(); it++) {
      // Try to match regular expression with argument
      if (std::regex_search(it->c_str(), m, regex)) {
        SWARM_ASSERT(it + offset < list.end(), "Error out-of-range after applying offset in get first param match");
        return *(it + offset);
      }
    }
    return "";
  }

  std::string get_last_param() const { return list.back(); }
  void        append(const std::string& str) { list.emplace_back(str); }

  void substitute_all_param_match(const std::string& regexp, const std::string& str, std::size_t offset = 0)
  {
    std::regex  regex(regexp);
    std::cmatch m;

    // For each argument
    for (auto it = list.begin(); it != list.end(); it++) {
      // Try to match regular expression with argument
      if (std::regex_search(it->c_str(), m, regex)) {
        SWARM_ASSERT(it + offset < list.end(),
                     "Error out-of-range after applying offset in substitute all parameter match");
        *(it + offset) = str;
      }
    }
  }
};
} // namespace swarm
#endif // SWARM_ARGS_H
