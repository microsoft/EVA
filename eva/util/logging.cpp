// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/util/logging.h"
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdlib>

namespace eva {

int getUserVerbosity() {
  static int userVerbosity = 0;
  static bool parsed = false;
  if (!parsed) {
    if (const char *envP = std::getenv("EVA_VERBOSITY")) {
      auto envStr = std::string(envP);
      try {
        userVerbosity = std::stoi(envStr);
      } catch (std::invalid_argument e) {
        std::transform(envStr.begin(), envStr.end(), envStr.begin(), ::tolower);
        if (envStr == "silent") {
          userVerbosity = 0;
        } else if (envStr == "info") {
          userVerbosity = (int)Verbosity::Info;
        } else if (envStr == "debug") {
          userVerbosity = (int)Verbosity::Debug;
        } else if (envStr == "trace") {
          userVerbosity = (int)Verbosity::Trace;
        } else {
          std::cerr << "Invalid verbosity EVA_VERBOSITY=" << envStr
                    << " Defaulting to silent.\n";
          userVerbosity = 0;
        }
      }
    }
    parsed = true;
  }
  return userVerbosity;
}

void log(Verbosity verbosity, const char *fmt, ...) {
  if (getUserVerbosity() >= (int)verbosity) {
    printf("EVA: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
  }
}

bool verbosityAtLeast(Verbosity verbosity) {
  return getUserVerbosity() >= (int)verbosity;
}

void warn(const char *fmt, ...) {
  fprintf(stderr, "WARNING: ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  fflush(stderr);
}

} // namespace eva
