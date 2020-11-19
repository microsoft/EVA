// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ckks/ckks_config.h"
#include "eva/util/logging.h"
#include <sstream>

namespace eva {

CKKSConfig::CKKSConfig(
    const std::unordered_map<std::string, std::string> &configMap) {
  for (const auto &entry : configMap) {
    const auto &option = entry.first;
    const auto &valueStr = entry.second;
    if (option == "balance_reductions") {
      std::istringstream is(valueStr);
      is >> std::boolalpha >> balanceReductions;
      if (is.bad()) {
        warn("Could not parse boolean in balance_reductions=%s. Falling back "
             "to default.",
             valueStr.c_str());
      }
    } else if (option == "rescaler") {
      if (valueStr == "lazy_waterline") {
        rescaler = CKKSRescaler::LazyWaterline;
      } else if (valueStr == "eager_waterline") {
        rescaler = CKKSRescaler::EagerWaterline;
      } else if (valueStr == "always") {
        rescaler = CKKSRescaler::Always;
      } else if (valueStr == "minimum") {
        rescaler = CKKSRescaler::Minimum;
      } else {
        // Please update this warning message when adding new options to the
        // cases above
        warn("Unknown value rescaler=%s. Available rescalers are "
             "lazy_waterline, eager_waterline, always, minimum. Falling back "
             "to default.",
             valueStr.c_str());
      }
    } else if (option == "lazy_relinearize") {
      std::istringstream is(valueStr);
      is >> std::boolalpha >> lazyRelinearize;
      if (is.bad()) {
        warn("Could not parse boolean in lazy_relinearize=%s. Falling back to "
             "default.",
             valueStr.c_str());
      }
    } else if (option == "security_level") {
      std::istringstream is(valueStr);
      is >> securityLevel;
      if (is.bad()) {
        throw std::runtime_error(
            "Could not parse unsigned int in security_level=" + valueStr);
      }
    } else if (option == "quantum_safe") {
      std::istringstream is(valueStr);
      is >> std::boolalpha >> quantumSafe;
      if (is.bad()) {
        throw std::runtime_error("Could not parse boolean in quantum_safe=" +
                                 valueStr);
      }
    } else if (option == "warn_vec_size") {
      std::istringstream is(valueStr);
      is >> std::boolalpha >> warnVecSize;
      if (is.bad()) {
        warn("Could not parse boolean in warn_vec_size=%s. Falling "
             "back to default.",
             valueStr.c_str());
      }
    } else {
      warn("Unknown option %s. Available options are:\n%s", option.c_str(),
           OPTIONS_HELP_MESSAGE);
    }
  }
}

std::string CKKSConfig::toString(int indent) const {
  auto indentStr = std::string(indent, ' ');
  std::stringstream s;
  s << std::boolalpha;
  s << indentStr << "balance_reductions = " << balanceReductions;
  s << '\n';
  s << indentStr << "rescaler = ";
  switch (rescaler) {
  case CKKSRescaler::LazyWaterline:
    s << "lazy_waterline";
    break;
  case CKKSRescaler::EagerWaterline:
    s << "eager_waterline";
    break;
  case CKKSRescaler::Always:
    s << "always";
    break;
  case CKKSRescaler::Minimum:
    s << "minimum";
    break;
  }
  s << '\n';
  s << indentStr << "lazy_relinearize = " << lazyRelinearize;
  s << '\n';
  s << indentStr << "security_level = " << securityLevel;
  s << '\n';
  s << indentStr << "quantum_safe = " << quantumSafe;
  s << '\n';
  s << indentStr << "warn_vec_size = " << warnVecSize;
  return s.str();
}

} // namespace eva
