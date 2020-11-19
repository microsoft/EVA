// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <set>

namespace eva {

class RotationKeysSelector {
public:
  RotationKeysSelector(Program &g, const TermMap<Type> &type)
      : program_(g), type(type) {}

  void operator()(const Term::Ptr &term) {
    auto op = term->op;

    // Nothing to do if this is not a rotation
    if (!isLeftRotationOp(op) && !isRightRotationOp(op)) return;

    // No rotation keys needed for raw computation
    if (type[term] == Type::Raw) return;

    // Add the rotation count
    auto rotation = term->get<RotationAttribute>();
    keys_.insert(isRightRotationOp(op) ? -rotation : rotation);
  }

  void free(const Term::Ptr &term) {
    // No-op
  }

  auto getRotationKeys() {
    // Return the set of rotations needed
    return keys_;
  }

private:
  Program &program_;
  const TermMap<Type> &type;
  std::set<int> keys_;

  bool isLeftRotationOp(const Op &op_code) {
    return (op_code == Op::RotateLeftConst);
  }

  bool isRightRotationOp(const Op &op_code) {
    return (op_code == Op::RotateRightConst);
  }
};

} // namespace eva
