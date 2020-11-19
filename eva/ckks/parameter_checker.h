// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eva {

class InconsistentParameters : public std::runtime_error {
public:
  InconsistentParameters(const std::string &msg) : std::runtime_error(msg) {}
};

class ParameterChecker {
  TermMap<Type> &types;

public:
  ParameterChecker(Program &g, TermMap<Type> &types)
      : program_(g), parms_(g), types(types) {}

  void operator()(const Term::Ptr &term) {
    // Must only be used with forward pass traversal
    auto &operands = term->getOperands();
    if (types[term] == Type::Raw || term->op == Op::Encode) {
      return;
    }
    if (operands.size() > 0) {
      // Get the parameters for this term
      auto &parms = parms_[term];
      // Loop over operands
      for (auto &operand : operands) {
        // Get the parameters for the operand
        auto &operandParms = parms_[operand];

        // Nothing to do if the operand parameters are empty; the operand sets
        // no requirements on this node
        if (operandParms.size() > 0) {
          if (parms.size() > 0) {
            // If the parameters for this term are already set (from a different
            // operand), they must match the current operand's parameters
            if (operandParms.size() != parms.size()) {
              throw InconsistentParameters(
                  "Two operands require different number of primes");
            }

            // Loop over the primes in the parameters for this term
            for (std::size_t i = 0; i < parms.size(); ++i) {
              if (parms[i] == 0) {
                // If any of the primes is zero (indicating a previous modulus
                // switch operand term, fill in its true value from the current
                // operand
                parms[i] = operandParms[i];
              } else if (operandParms[i] != 0) {
                // If the operand prime is non-zero, require equality
                if (parms[i] != operandParms[i]) {
                  throw InconsistentParameters(
                      "Primes required by two operands do not match");
                }
              }
            }
          } else {
            // This is the first operand to impose conditions on this term;
            // copy the parameters from the operand
            parms = operandParms;
          }
        }
      }

      if (isModSwitchOp(term->op)) {
        // Is this a modulus switch? If so, add an extra (placeholder) zero
        parms.push_back(0);
      } else if (isRescaleOp(term->op)) {
        // Is this a rescale? Then add a prime of the requested size
        auto divisor = term->get<RescaleDivisorAttribute>();
        assert(divisor != 0);
        parms.push_back(divisor);
      }
    } else {
      // Get the parameters for this term
      auto &parms = parms_[term];
      std::uint32_t level = term->get<EncodeAtLevelAttribute>();
      while (level > 0) {
        parms.push_back(0);
        level--;
      }
    }
  }

  void free(const Term::Ptr &term) { parms_[term].clear(); }

private:
  Program &program_;
  TermMap<std::vector<std::uint32_t>> parms_;

  bool isModSwitchOp(const Op &op_code) { return (op_code == Op::ModSwitch); }

  bool isRescaleOp(const Op &op_code) { return (op_code == Op::Rescale); }
};

} // namespace eva
