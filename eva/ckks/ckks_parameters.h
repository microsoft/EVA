// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/serialization/ckks.pb.h"
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace eva {

struct CKKSParameters {
  std::vector<std::uint32_t> primeBits; // in log-scale
  std::set<int> rotations;
  std::uint32_t polyModulusDegree;
};

std::unique_ptr<msg::CKKSParameters> serialize(const CKKSParameters &);
std::unique_ptr<CKKSParameters> deserialize(const msg::CKKSParameters &);

} // namespace eva
