// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <stdexcept>
#include <string>

namespace eva {

#define EVA_OPS                                                                \
  X(Undef, 0)                                                                  \
  X(Input, 1)                                                                  \
  X(Output, 2)                                                                 \
  X(Constant, 3)                                                               \
  X(Negate, 10)                                                                \
  X(Add, 11)                                                                   \
  X(Sub, 12)                                                                   \
  X(Mul, 13)                                                                   \
  X(RotateLeftConst, 14)                                                       \
  X(RotateRightConst, 15)                                                      \
  X(Relinearize, 20)                                                           \
  X(ModSwitch, 21)                                                             \
  X(Rescale, 22)                                                               \
  X(Encode, 23)

enum class Op {
#define X(op, code) op = code,
  EVA_OPS
#undef X
};

inline bool isValidOp(Op op) {
  switch (op) {
#define X(op, code) case Op::op:
    EVA_OPS
#undef X
    return true;
  default:
    return false;
  }
}

inline std::string getOpName(Op op) {
  switch (op) {
#define X(op, code)                                                            \
  case Op::op:                                                                 \
    return #op;
    EVA_OPS
#undef X
  default:
    throw std::runtime_error("Invalid op");
  }
}

} // namespace eva
