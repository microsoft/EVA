// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/ckks_parameters.h"
#include "eva/ir/program.h"
#include "eva/seal/seal.h"
#include "eva/serialization/known_type.pb.h"
#include "eva/util/overloaded.h"
#include <memory>
#include <stdexcept>
#include <variant>

#define EVA_KNOWN_TYPE_TRY_DESERIALIZE(MsgType)                                \
  do {                                                                         \
    if (msg.contents().Is<MsgType>()) {                                        \
      MsgType inner;                                                           \
      if (!msg.contents().UnpackTo(&inner)) {                                  \
        throw std::runtime_error("Unpacking inner message failed");            \
      }                                                                        \
      obj = deserialize(inner);                                                \
      return;                                                                  \
    }                                                                          \
  } while (false)

namespace eva {

// Represents any serializable EVA object
using KnownType =
    std::variant<std::unique_ptr<Program>, std::unique_ptr<CKKSParameters>,
                 std::unique_ptr<CKKSSignature>, std::unique_ptr<SEALValuation>,
                 std::unique_ptr<SEALPublic>, std::unique_ptr<SEALSecret>>;

KnownType deserialize(const msg::KnownType &msg);

} // namespace eva
