// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/common/valuation.h"
#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace eva {

// Executes unencrypted computation
class ReferenceExecutor {
public:
  ReferenceExecutor(Program &g)
      : program_(g), vecSize_(g.getVecSize()), terms_(g) {}

  ReferenceExecutor(const ReferenceExecutor &copy) = delete;

  ReferenceExecutor &operator=(const ReferenceExecutor &assign) = delete;

  template <typename InputType>
  void setInputs(const std::unordered_map<std::string, InputType> &inputs) {
    for (auto &in : inputs) {
      auto term = program_.getInput(in.first);
      terms_[term] = in.second; // TODO: can we avoid this copy?
      if (terms_[term].size() != vecSize_) {
        throw std::runtime_error(
            "The length of all inputs must be the same as program's vector "
            "size. Input " +
            in.first + " has length " + std::to_string(terms_[term].size()) +
            ", but vector size is " + std::to_string(vecSize_));
      }
    }
  }

  void operator()(const Term::Ptr &term);

  void free(const Term::Ptr &term) {
    if (term->op == Op::Output) return;
    terms_[term].clear();
  }

  void getOutputs(Valuation &outputs) {
    for (auto &out : program_.getOutputs()) {
      outputs[out.first] = terms_[out.second];
    }
  }

private:
  Program &program_;
  std::uint64_t vecSize_;
  TermMapOptional<std::vector<double>> terms_;

  template <class Op>
  void binOp(std::vector<double> &out, const Term::Ptr &args1,
             const Term::Ptr &args2) {
    auto &in1 = terms_.at(args1);
    auto &in2 = terms_.at(args2);
    assert(in1.size() == in2.size());

    out.clear();
    out.reserve(in1.size());
    transform(in1.cbegin(), in1.cend(), in2.cbegin(), back_inserter(out), Op());
  }

  void leftRotate(std::vector<double> &output, const Term::Ptr &args,
                  std::int32_t shift);

  void rightRotate(std::vector<double> &output, const Term::Ptr &args,
                   std::int32_t shift);

  void negate(std::vector<double> &output, const Term::Ptr &args);
};

} // namespace eva
