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

#ifndef SWARM_STRING_HELPERS_H
#define SWARM_STRING_HELPERS_H

#include <sstream>
#include <string>
#include <vector>

namespace swarm {
namespace string_helpers {
static inline std::vector<std::string> split(const std::string& str, char delimiter)
{
  std::vector<std::string> list;

  // Split string
  std::stringstream ss(str);
  while (ss.good()) {
    std::string substr;
    std::getline(ss, substr, delimiter);

    if (not substr.empty()) {
      list.emplace_back(substr);
    }
  }

  return list;
}
} // namespace string_helpers
} // namespace swarm
#endif // SWARM_STRING_HELPERS_H
