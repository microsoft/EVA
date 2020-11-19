// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/rescaler.h"
#include "eva/util/logging.h"

namespace eva {

class MinimumRescaler : public Rescaler {
  std::uint32_t minScale;
  const std::uint32_t maxRescale = 60;

public:
  MinimumRescaler(Program &g, TermMap<Type> &type,
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
    auto &operands = term->getOperands();
    if (operands.size() == 0) return; // inputs
    if (type[term] == Type::Raw) {
      handleRawScale(term);
      return;
    }

    auto op = term->op;

    if (isRescaleOp(op)) return; // already processed

    if (!isMultiplicationOp(op)) {
      // Op::Add, Op::Sub, NEGATE, COPY, Op::RotateLeftConst,
      // Op::RotateRightConst copy scale of the first operand
      for (auto &operand : operands) {
        assert(operand->op != Op::Constant);
        assert(scale[operand] != 0);
        scale[term] = scale[operand];
        break;
      }
      if (isAdditionOp(op)) {
        // Op::Add, Op::Sub
        auto maxScale = scale[term];
        for (auto &operand : operands) {
          // Here we allow raw operands to possibly raise the scale
          if (scale[operand] > maxScale) maxScale = scale[operand];
        }
        for (auto &operand : operands) {
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
            // iteration.
            //       Refine API to make this less surprising.
            term->replaceOperand(operand, mulNode);
          }
        }
        // assert that all operands have the same scale
        for (auto &operand : operands) {
          assert(maxScale == scale[operand] || type[operand] == Type::Raw);
        }
        scale[term] = maxScale;
      }
      return;
    }

    // Op::Multiply only
    // ASSUME: only two operands
    std::vector<Term::Ptr> operandsCopy;
    for (auto &operand : operands) {
      operandsCopy.push_back(operand);
    }
    assert(operandsCopy.size() == 2);
    std::uint32_t multScale = scale[operandsCopy[0]] + scale[operandsCopy[1]];
    assert(multScale != 0);
    scale[term] = multScale;

    auto minOfScales = scale[operandsCopy[0]];
    if (minOfScales > scale[operandsCopy[1]])
      minOfScales = scale[operandsCopy[1]];
    auto rescaleBy = minOfScales - minScale;
    if (rescaleBy > maxRescale) rescaleBy = maxRescale;
    if ((2 * rescaleBy) >= maxRescale) {
      // rescale after multiplication is inevitable
      // to reduce the growth of scale, rescale both operands before
      // multiplication
      assert(rescaleBy <= maxRescale);
      insertRescaleBetween(operandsCopy[0], term, rescaleBy);
      if (operandsCopy[0] != operandsCopy[1]) {
        insertRescaleBetween(operandsCopy[1], term, rescaleBy);
      }

      scale[term] = multScale - (2 * rescaleBy);
    } else {
      // rescale only if above the waterline
      auto temp = term;
      while (multScale >= (maxRescale + minScale)) {
        temp = insertRescale(temp, maxRescale);
        multScale -= maxRescale;
        assert(multScale == scale[temp]);
      }
    }
  }
};

} // namespace eva
