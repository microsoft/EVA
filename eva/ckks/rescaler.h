// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class Rescaler {
protected:
  Program &program;
  TermMap<Type> &type;
  TermMapOptional<std::uint32_t> &scale;

  Rescaler(Program &g, TermMap<Type> &type,
           TermMapOptional<std::uint32_t> &scale)
      : program(g), type(type), scale(scale) {}

  bool isRescaleOp(const Op &op_code) { return (op_code == Op::Rescale); }

  bool isMultiplicationOp(const Op &op_code) { return (op_code == Op::Mul); }

  bool isAdditionOp(const Op &op_code) {
    return ((op_code == Op::Add) || (op_code == Op::Sub));
  }

  auto insertRescale(Term::Ptr term, std::uint32_t rescaleBy) {
    // auto scale = term->getScale();
    auto rescaleNode = program.makeRescale(term, rescaleBy);
    type[rescaleNode] = type[term];
    scale[rescaleNode] = scale[term] - rescaleBy;

    term->replaceOtherUsesWith(rescaleNode);

    return rescaleNode;
  }

  void insertRescaleBetween(Term::Ptr term1, Term::Ptr term2,
                            std::uint32_t rescaleBy) {
    auto rescaleNode = program.makeRescale(term1, rescaleBy);
    type[rescaleNode] = type[term1];
    scale[rescaleNode] = scale[term1] - rescaleBy;

    term2->replaceOperand(term1, rescaleNode);
  }

  void handleRawScale(Term::Ptr term) {
    if (term->numOperands() > 0) {
      int maxScale = 0;
      for (auto &operand : term->getOperands()) {
        if (scale.at(operand) > maxScale) maxScale = scale.at(operand);
      }
      scale[term] = maxScale;
    }
  }
};

} // namespace eva
