// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <string>
#include <unordered_map>

namespace eva {

// clang-format off
const char *const OPTIONS_HELP_MESSAGE =
    "balance_reductions - Balance trees of mul, add or sub operations. bool (default=true)\n"
    "rescaler           - Rescaling policy. One of: lazy_waterline (default), eager_waterline, always, minimum\n"
    "lazy_relinearize   - Relinearize as late as possible. bool (default=true)\n"
    "security_level     - How many bits of security parameters should be selected for. int (default=128)\n"
    "quantum_safe       - Select quantum safe parameters. bool (default=false)\n"
    "warn_vec_size      - Warn about possibly inefficient vector size selection. bool (default=true)";
// clang-format on

enum class CKKSRescaler { LazyWaterline, EagerWaterline, Always, Minimum };

// Controls the behavior of CKKSCompiler
class CKKSConfig {
public:
  CKKSConfig() {}
  CKKSConfig(const std::unordered_map<std::string, std::string> &configMap);

  std::string toString(int indent = 0) const;

  bool balanceReductions = true;
  CKKSRescaler rescaler = CKKSRescaler::LazyWaterline;
  bool lazyRelinearize = true;
  uint32_t securityLevel = 128;
  bool quantumSafe = false;

  // Warnings
  bool warnVecSize = true;
};

} // namespace eva
