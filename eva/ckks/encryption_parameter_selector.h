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

class EncryptionParametersSelector {
public:
  EncryptionParametersSelector(Program &g,
                               TermMapOptional<std::uint32_t> &scales,
                               TermMap<Type> &types)
      : program_(g), scales_(scales), terms_(g), types(types) {}

  void operator()(const Term::Ptr &term) {
    // This function computes, for each term, the set of coeff_modulus primes
    // needed to reach that term, taking only into account rescalings. Primes
    // needed to hold the output values are not included. For example, input
    // terms require no extra primes, so for input terms this function will
    // assign an empty set of primes. The example below shows parameters
    // assigned in a simple example computation, where we rescale by 40 bits:
    //
    //             In_1:{}   In_2:{}   In_3:{}
    //                  \          \   /
    //                   \          \ /
    //                    \          * MULTIPLY:{}
    //                     \         |
    //                      \        |
    //                       \       * RESCALE:{40}
    //                        \      |
    //                         \     |
    //                          -----* ADD:{40}
    //                               |
    //                               |
    //                           Out_1:{40}

    // This function must only be used with forward pass traversal, as it
    // expects operand terms to have been processed already.
    if (types[term] == Type::Raw || term->op == Op::Encode) {
      return;
    }
    auto &operands = term->getOperands();

    // Nothing to do for inputs
    if (operands.size() > 0) {
      // Get the parameters for this term
      auto &parms = terms_[term];

      for (auto &operand : operands) {
        // Get the parameters for each operand (forward pass)
        auto &operandParms = terms_[operand];

        // Set the parameters for this term to be the maximum over operands
        if (operandParms.size() > parms.size()) {
          parms = operandParms;
        }
      }

      // Adjust the parameters if this term is a rescale operation
      // NOTE: This is ignoring modulus switches, but still works because there
      // is always a longest path with no modulus switches.
      // TODO: Validate this claim and generalize to include modulus switches.
      if (isRescaleOp(term->op)) {
        auto newSize = parms.size() + 1;

        // By how much are we rescaling?
        auto divisor = term->get<RescaleDivisorAttribute>();
        assert(divisor != 0);

        // Add the required scaling factor to the parameters
        parms.push_back(divisor);
        assert(parms.size() == newSize);
      }
    }
  }

  inline void free(const Term::Ptr &term) { terms_[term].clear(); }

  auto getEncryptionParameters() {
    // This function returns the encryption parameters (really just a list of
    // prime bit counts for the coeff_modulus) needed for this computation. It
    // can be called after forward pass traversal has computed the rescaling
    // primes for all terms.
    //
    // The logic is simple: we loop over each output term as those have the
    // largest (largest number of primes) parameter sets after forward
    // traversal, and find the largest parameter set among those. This set will
    // work globally for the computation. Since the parameters are not taking
    // into account the need for storing the result for the output terms, we
    // need to add one or more additional primes to the parameters, depending on
    // the scales and the ranges of the terms. For example, if the output term
    // has a parameter set {40} after forward traversal, with a scale and range
    // of 40 and 16 bits, respectively, the result requires an additional 56-bit
    // prime in the parameter set. This prime is always added in the set before
    // the rescaling primes, so in this case the function would return {56,40}.
    // If the scale and range are very large, this function will add more than
    // one extra prime.

    std::vector<std::uint32_t> parms;

    // The size in bits needed for the output value; this includes the scale and
    // the range
    std::uint32_t maxOutputSize = 0;

    // The bit count of the largest prime appearing in the parameters
    std::uint32_t maxParm = 0;

    // The largest (largest number of primes) set of parameters required among
    // all output terms
    std::uint32_t maxLen = 0;

    // Loop over each output term
    for (auto &entry : program_.getOutputs()) {
      auto &output = entry.second;

      // The size for this output term equals the range attribute (bits) plus
      // the scale (bits)
      auto size = output->get<RangeAttribute>();
      size += scales_[output];

      // Update maxOutputSize
      if (size > maxOutputSize) maxOutputSize = size;

      // Get the parameters for the current output term
      auto &oParms = terms_[output];

      // Update maxLen (number of primes)
      if (maxLen < oParms.size()) maxLen = oParms.size();

      // Update maxParm (largest prime)
      for (auto &parm : oParms) {
        if (parm > maxParm) maxParm = parm;
      }
    }

    // Ensure that the output size is non-zero
    assert(maxOutputSize != 0);

    if (maxOutputSize > 60) {
      // If the required output size is larger than 60 bits, we need to increase
      // the parameters with more than one additional primes.

      // In this case maxPrime is always 60 bits
      maxParm = 60;

      // Add 60-bit primes for as long as needed
      while (maxOutputSize >= 60) {
        parms.push_back(60);
        maxOutputSize -= 60;
      }

      // Add one more prime if needed
      if (maxOutputSize > 0) {
        // TODO: The minimum should probably depend on poly_modulus_degree
        parms.push_back(std::max(20u, maxOutputSize));
      }
    } else {
      // The output size is less than 60 bits so the output parameters require
      // only one additional prime.

      // Update maxParm
      if (maxOutputSize > maxParm) maxParm = maxOutputSize;

      // Add the required prime to the parameters for this term
      parms.push_back(maxParm);
    }

    // Finally, loop over all output terms and add the largest parameter set to
    // parms after what was pushed above.
    for (auto &entry : program_.getOutputs()) {
      auto &output = entry.second;

      // Get the parameters for the current output term
      auto &oParms = terms_[output];

      // If this output node has the longest parameter set, use it
      if (maxLen == oParms.size()) {
        parms.insert(parms.end(), oParms.rbegin(), oParms.rend());

        // Exit the for loop; we have our parameter set
        break;
      }
    }

    // Add maxParm to result parameters; this is the "key prime".
    // TODO: This might be too aggressive. We can try smaller primes here as
    // well, which in some cases is advantageous as it may result in smaller
    // poly_modulus_degree, even though the noise growth may be a bit larger.
    parms.push_back(maxParm);

    return parms;
  }

private:
  Program &program_;
  TermMapOptional<std::uint32_t> &scales_;
  TermMap<std::vector<std::uint32_t>> terms_;
  TermMap<Type> &types;

  inline bool isRescaleOp(const Op &op_code) { return op_code == Op::Rescale; }
};

} // namespace eva
