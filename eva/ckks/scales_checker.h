// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace eva {

class ScalesChecker {
public:
  ScalesChecker(Program &g, TermMapOptional<std::uint32_t> &scales,
                TermMap<Type> &types)
      : program_(g), scales_(g), types_(types) {}

  void operator()(const Term::Ptr &term) {
    // Must only be used with forward pass traversal
    if (types_[term] == Type::Raw) {
      return;
    }
    auto &operands = term->getOperands();

    // Nothing to do for source terms
    if (term->op == Op::Input || term->op == Op::Encode) {
      scales_[term] = term->get<EncodeAtScaleAttribute>();
      if (scales_.at(term) == 0) {
        if (term->op == Op::Input) {
          throw std::runtime_error("Program has an input with 0 scale");
        } else {
          throw std::logic_error("Compiled program results in a 0 scale term");
        }
      }
    } else if (term->op == Op::Mul) {
      assert(term->numOperands() == 2);
      std::uint32_t scale = 0;
      for (auto &operand : operands) {
        scale += scales_.at(operand);
      }
      if (scale == 0) {
        throw std::logic_error("Compiled program results in a 0 scale term");
      }
      scales_[term] = scale;
    } else if (term->op == Op::Rescale) {
      assert(term->numOperands() == 1);
      auto divisor = term->get<RescaleDivisorAttribute>();
      auto operandScale = scales_.at(term->operandAt(0));
      std::uint32_t scale = operandScale - divisor;
      if (scale == 0) {
        throw std::logic_error("Compiled program results in a 0 scale term");
      }
      scales_[term] = scale;

    } else if (isAdditionOp(term->op)) {
      std::uint32_t scale = 0;
      for (auto &operand : operands) {
        if (scale == 0) {
          scale = scales_.at(operand);
        } else {
          if (scale != scales_.at(operand)) {
            throw std::logic_error("Addition or subtraction in program has "
                                   "operands of non-equal scale");
          }
        }
      }
      if (scale == 0) {
        throw std::logic_error("Compiled program results in a 0 scale term");
      }
      scales_[term] = scale;
    } else {
      auto scale = scales_.at(term->operandAt(0));
      if (scale == 0) {
        throw std::logic_error("Compiled program results in a 0 scale term");
      }
      scales_[term] = scale;
    }
  }

  void free(const Term::Ptr &term) {
    // No-op
  }

private:
  Program &program_;
  TermMapOptional<std::uint32_t> scales_;
  TermMap<Type> &types_;

  bool isAdditionOp(const Op &op_code) {
    return ((op_code == Op::Add) || (op_code == Op::Sub));
  }
};

} // namespace eva
