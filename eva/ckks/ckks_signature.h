// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/types.h"
#include "eva/serialization/ckks.pb.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace eva {

// TODO: make these structs immutable

struct CKKSEncodingInfo {
  Type inputType;
  int scale;
  int level;

  CKKSEncodingInfo(Type inputType, int scale, int level)
      : inputType(inputType), scale(scale), level(level) {}
};

struct CKKSSignature {
  int vecSize;
  std::unordered_map<std::string, CKKSEncodingInfo> inputs;

  CKKSSignature(int vecSize,
                std::unordered_map<std::string, CKKSEncodingInfo> inputs)
      : vecSize(vecSize), inputs(inputs) {}
};

std::unique_ptr<msg::CKKSSignature> serialize(const CKKSSignature &);
std::unique_ptr<CKKSSignature> deserialize(const msg::CKKSSignature &);

} // namespace eva
