// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <stdexcept>
#include <string>

namespace eva {

#define EVA_TYPES                                                              \
  X(Undef, 0)                                                                  \
  X(Cipher, 1)                                                                 \
  X(Raw, 2)                                                                    \
  X(Plain, 3)

enum class Type : std::int32_t {
#define X(type, code) type = code,
  EVA_TYPES
#undef X
};

inline std::string getTypeName(Type type) {
  switch (type) {
#define X(type, code)                                                          \
  case Type::type:                                                             \
    return #type;
    EVA_TYPES
#undef X
  default:
    throw std::runtime_error("Invalid type");
  }
}

} // namespace eva
