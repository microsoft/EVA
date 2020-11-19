// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <functional>
#include <iostream>
#include <string>

namespace eva {

enum class Verbosity {
  Info = 1,
  Debug = 2,
  Trace = 3,
};

void log(Verbosity verbosity, const char *fmt, ...);
bool verbosityAtLeast(Verbosity verbosity);
void warn(const char *fmt, ...);

} // namespace eva
