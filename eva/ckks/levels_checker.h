// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace eva {

class LevelsChecker {
public:
  LevelsChecker(Program &g, TermMap<Type> &types)
      : program_(g), types_(types), levels_(g) {}

  void operator()(const Term::Ptr &term) {
    // This function verifies that the levels are compatibile. It assumes the
    // operand terms are processed already, so it must only be used with forward
    // pass traversal.

    if (term->numOperands() == 0) {
      // If this is a source node, get the encoding level
      levels_[term] = term->get<EncodeAtLevelAttribute>();
    } else {
      // For other terms, the operands must all have matching level. First find
      // the level of any of the ciphertext operands.
      std::size_t operandLevel;
      for (auto &operand : term->getOperands()) {
        if (types_[operand] == Type::Cipher) {
          operandLevel = levels_[operand];
          break;
        }
      }

      // Next verify that all operands have the same level.
      for (auto &operand : term->getOperands()) {
        if (types_[operand] == Type::Cipher) {
          auto operandLevel2 = levels_[operand];
          assert(operandLevel == operandLevel2);
        }
      }

      // Incremenet the level for a rescale or modulus switch
      std::size_t level = operandLevel;
      if (isRescaleOp(term->op) || isModSwitchOp(term->op)) {
        ++level;
      }
      levels_[term] = level;
    }
  }

  void free(const Term::Ptr &term) {
    // No-op
  }

private:
  Program &program_;
  TermMap<Type> &types_;

  // Maintains the reverse level (leaves have 0, roots have max)
  TermMap<std::size_t> levels_;

  bool isModSwitchOp(const Op &op_code) { return (op_code == Op::ModSwitch); }

  bool isRescaleOp(const Op &op_code) { return (op_code == Op::Rescale); }
};

} // namespace eva
