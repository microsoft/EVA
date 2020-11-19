// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/rescaler.h"
#include "eva/util/logging.h"

namespace eva {

class EagerWaterlineRescaler : public Rescaler {
  std::uint32_t minScale;
  const std::uint32_t fixedRescale = 60;

public:
  EagerWaterlineRescaler(Program &g, TermMap<Type> &type,
                         TermMapOptional<std::uint32_t> &scale)
      : Rescaler(g, type, scale) {
    // ASSUME: minScale is max among all the inputs' scale
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
        auto maxScale = scale[term];
        for (auto &operand : term->getOperands()) {
          // Here we allow raw operands to possibly raise the scale
          if (scale[operand] > maxScale) maxScale = scale[operand];
        }
        for (auto &operand : term->getOperands()) {
          if (scale[operand] < maxScale && type[operand] != Type::Raw) {
            log(Verbosity::Trace,
                "Scaling up t%i from scale %i to match other addition operands "
                "at scale %i",
                operand->index, scale[operand], maxScale);

            auto scaleConstant = program.makeUniformConstant(1);
            scale[scaleConstant] = maxScale - scale[operand];
            scaleConstant->set<EncodeAtScaleAttribute>(scale[scaleConstant]);

            auto mulNode = program.makeTerm(Op::Mul, {operand, scaleConstant});
            scale[mulNode] = maxScale;

            // TODO: Not obviously correct as it's modifying inside
            // iteration. Refine API to make this less surprising.
            term->replaceOperand(operand, mulNode);
          }
        }
        // assert that all operands have the same scale
        for (auto &operand : term->getOperands()) {
          assert(maxScale == scale[operand] || type[operand] == Type::Raw);
        }
        scale[term] = maxScale;
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

    // rescale only if above the waterline
    auto temp = term;
    while (multScale >= (fixedRescale + minScale)) {
      temp = insertRescale(temp, fixedRescale);
      multScale -= fixedRescale;
      assert(multScale == scale[temp]);
    }
  }
};

} // namespace eva
