// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/common/reference_executor.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <numeric>

using namespace std;

namespace eva {

void ReferenceExecutor::leftRotate(vector<double> &output,
                                   const Term::Ptr &args, int32_t shift) {
  auto &input = terms_.at(args);

  // Reserve enough space for output
  output.clear();
  output.reserve(input.size());

  while (shift > 0 && shift >= input.size())
    shift -= input.size();
  while (shift < 0)
    shift += input.size();

  // Shift left and copy to output
  copy_n(input.cbegin() + shift, input.size() - shift, back_inserter(output));
  copy_n(input.cbegin(), shift, back_inserter(output));
}

void ReferenceExecutor::rightRotate(vector<double> &output,
                                    const Term::Ptr &args, int32_t shift) {
  auto &input = terms_.at(args);

  // Reserve enough space for output
  output.clear();
  output.reserve(input.size());

  while (shift > 0 && shift >= input.size())
    shift -= input.size();
  while (shift < 0)
    shift += input.size();

  // Shift right and copy to output
  copy_n(input.cend() - shift, shift, back_inserter(output));
  copy_n(input.cbegin(), input.size() - shift, back_inserter(output));
}

void ReferenceExecutor::negate(vector<double> &output, const Term::Ptr &args) {
  auto &input = terms_.at(args);

  // Reserve enough space for output
  output.clear();
  output.reserve(input.size());
  transform(input.cbegin(), input.cend(), back_inserter(output),
            std::negate<double>());
}

void ReferenceExecutor::operator()(const Term::Ptr &term) {
  // Must only be used with forward pass traversal
  auto &output = terms_[term];

  auto op = term->op;
  auto args = term->getOperands();

  switch (op) {
  case Op::Input:
    // Nothing to do for inputs
    break;
  case Op::Constant:
    // A constant (vector) is expanded to the number of slots (vecSize_ here)
    term->get<ConstantValueAttribute>()->expandTo(output, vecSize_);
    break;
  case Op::Add:
    assert(args.size() == 2);
    binOp<std::plus<double>>(output, args[0], args[1]);
    break;
  case Op::Sub:
    assert(args.size() == 2);
    binOp<std::minus<double>>(output, args[0], args[1]);
    break;
  case Op::Mul:
    assert(args.size() == 2);
    binOp<std::multiplies<double>>(output, args[0], args[1]);
    break;
  case Op::RotateLeftConst:
    assert(args.size() == 1);
    leftRotate(output, args[0], term->get<RotationAttribute>());
    break;
  case Op::RotateRightConst:
    assert(args.size() == 1);
    rightRotate(output, args[0], term->get<RotationAttribute>());
    break;
  case Op::Negate:
    assert(args.size() == 1);
    negate(output, args[0]);
    break;
  case Op::Encode:
    [[fallthrough]];
  case Op::Output:
    [[fallthrough]];
  case Op::Relinearize:
    [[fallthrough]];
  case Op::ModSwitch:
    [[fallthrough]];
  case Op::Rescale:
    // Copy argument value for outputs
    assert(args.size() == 1);
    output = terms_[args[0]];
    break;
  default:
    assert(false);
  }
}

} // namespace eva
