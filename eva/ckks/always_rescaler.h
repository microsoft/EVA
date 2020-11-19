// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/rescaler.h"

namespace eva {

class AlwaysRescaler : public Rescaler {
  std::uint32_t minScale;

public:
  AlwaysRescaler(Program &g, TermMap<Type> &type,
                 TermMapOptional<std::uint32_t> &scale)
      : Rescaler(g, type, scale) {
    // ASSUME: minScale is max among all the cipher inputs' scale
    minScale = 0;
    for (auto &source : program.getSources()) {
      if (scale[source] > minScale) minScale = scale[source];
    }
    assert(minScale != 0);
  }

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    if (term->numOperands() == 0) return; // inputs
    if (type[term] == Type::Raw) {
      handleRawScale(term);
      return;
    }

    if (isRescaleOp(term->op)) return; // already processed

    if (!isMultiplicationOp(term->op)) {
      // Op::Add, Op::Sub, NEGATE, COPY, Op::RotateLeftConst,
      // Op::RotateRightConst copy scale of the first operand
      scale[term] = scale[term->operandAt(0)];
      if (isAdditionOp(term->op)) {
        // Op::Add, Op::Sub
        // assert that all operands have the same scale
        for (auto &operand : term->getOperands()) {
          if (type[operand] != Type::Raw) {
            assert(scale[term] == scale[operand] || type[operand] == Type::Raw);
          }
        }
      }
      return;
    }

    // Op::Mul only
    // ASSUME: only two operands
    std::uint32_t multScale = 0;
    for (auto &operand : term->getOperands()) {
      multScale += scale[operand];
    }
    assert(multScale != 0);
    scale[term] = multScale;

    // always rescale
    insertRescale(term, multScale - minScale);
  }
};

} // namespace eva
