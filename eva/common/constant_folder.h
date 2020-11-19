// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class ConstantFolder {
  Program &program;
  TermMapOptional<std::uint32_t> &scale;
  std::vector<double> scratch1, scratch2;

  bool isRescaleOp(const Op &op_code) { return (op_code == Op::Rescale); }

  bool isMultiplicationOp(const Op &op_code) { return (op_code == Op::Mul); }

  bool isAdditionOp(const Op &op_code) {
    return ((op_code == Op::Add) || (op_code == Op::Sub));
  }

  void replaceNodeWithConstant(Term::Ptr term,
                               const std::vector<double> &output,
                               double termScale) {
    // TODO: optimize output representations
    auto constant = program.makeDenseConstant(output);
    scale[constant] = termScale;
    constant->set<EncodeAtScaleAttribute>(scale[constant]);

    term->replaceAllUsesWith(constant);
    assert(term->numUses() == 0);
  }

  void add(Term::Ptr output, const Term::Ptr &args1, const Term::Ptr &args2) {
    auto &input1 = args1->get<ConstantValueAttribute>()->expand(
        scratch1, program.getVecSize());
    auto &input2 = args2->get<ConstantValueAttribute>()->expand(
        scratch2, program.getVecSize());

    std::vector<double> outputValue(input1.size());
    for (std::uint64_t i = 0; i < outputValue.size(); ++i) {
      outputValue[i] = input1[i] + input2[i];
    }

    replaceNodeWithConstant(output, outputValue,
                            std::max(scale[args1], scale[args2]));
  }

  void sub(Term::Ptr output, const Term::Ptr &args1, const Term::Ptr &args2) {
    auto &input1 = args1->get<ConstantValueAttribute>()->expand(
        scratch1, program.getVecSize());
    auto &input2 = args2->get<ConstantValueAttribute>()->expand(
        scratch2, program.getVecSize());

    std::vector<double> outputValue(input1.size());
    for (std::uint64_t i = 0; i < outputValue.size(); ++i) {
      outputValue[i] = input1[i] - input2[i];
    }

    replaceNodeWithConstant(output, outputValue,
                            std::max(scale[args1], scale[args2]));
  }

  void mul(Term::Ptr output, const Term::Ptr &args1, const Term::Ptr &args2) {
    auto &input1 = args1->get<ConstantValueAttribute>()->expand(
        scratch1, program.getVecSize());
    auto &input2 = args2->get<ConstantValueAttribute>()->expand(
        scratch2, program.getVecSize());

    std::vector<double> outputValue(input1.size());
    for (std::uint64_t i = 0; i < outputValue.size(); ++i) {
      outputValue[i] = input1[i] * input2[i];
    }

    replaceNodeWithConstant(output, outputValue,
                            std::max(scale[args1], scale[args2]));
  }

  void leftRotate(Term::Ptr output, const Term::Ptr &args1,
                  std::int32_t shift) {
    auto &input1 = args1->get<ConstantValueAttribute>()->expand(
        scratch1, program.getVecSize());

    while (shift > 0 && shift >= input1.size())
      shift -= input1.size();
    while (shift < 0)
      shift += input1.size();

    std::vector<double> outputValue(input1.size());
    for (std::uint64_t i = 0; i < (outputValue.size() - shift); ++i) {
      outputValue[i] = input1[i + shift];
    }
    for (std::uint64_t i = 0; i < shift; ++i) {
      outputValue[outputValue.size() - shift + i] = input1[i];
    }

    replaceNodeWithConstant(output, outputValue, scale[args1]);
  }

  void rightRotate(Term::Ptr output, const Term::Ptr &args1,
                   std::int32_t shift) {
    auto &input1 = args1->get<ConstantValueAttribute>()->expand(
        scratch1, program.getVecSize());

    while (shift > 0 && shift >= input1.size())
      shift -= input1.size();
    while (shift < 0)
      shift += input1.size();

    std::vector<double> outputValue(input1.size());
    for (std::uint64_t i = 0; i < (outputValue.size() - shift); ++i) {
      outputValue[i + shift] = input1[i];
    }
    for (std::uint64_t i = 0; i < shift; ++i) {
      outputValue[i] = input1[outputValue.size() - shift + i];
    }

    replaceNodeWithConstant(output, outputValue, scale[args1]);
  }

  void negate(Term::Ptr output, const Term::Ptr &args1) {
    auto &input1 = args1->get<ConstantValueAttribute>()->expand(
        scratch1, program.getVecSize());

    std::vector<double> outputValue(input1.size());
    for (std::uint64_t i = 0; i < outputValue.size(); ++i) {
      outputValue[i] = -input1[i];
    }

    replaceNodeWithConstant(output, outputValue, scale[args1]);
  }

public:
  ConstantFolder(Program &g, TermMapOptional<std::uint32_t> &scale)
      : program(g), scale(scale) {}

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    auto &args = term->getOperands();
    if (args.size() == 0) return; // inputs

    for (auto &arg : args) {
      if (arg->op != Op::Constant) return;
    }

    auto op_code = term->op;
    switch (op_code) {
    case Op::Add:
      assert(args.size() == 2);
      add(term, args[0], args[1]);
      break;
    case Op::Sub:
      assert(args.size() == 2);
      sub(term, args[0], args[1]);
      break;
    case Op::Mul:
      assert(args.size() == 2);
      mul(term, args[0], args[1]);
      break;
    case Op::RotateLeftConst:
      assert(args.size() == 1);
      leftRotate(term, args[0], term->get<RotationAttribute>());
      break;
    case Op::RotateRightConst:
      assert(args.size() == 1);
      rightRotate(term, args[0], term->get<RotationAttribute>());
      break;
    case Op::Negate:
      assert(args.size() == 1);
      negate(term, args[0]);
      break;
    case Op::Output:
      [[fallthrough]];
    case Op::Encode:
      break;
    case Op::Relinearize:
      [[fallthrough]];
    case Op::ModSwitch:
      [[fallthrough]];
    case Op::Rescale:
      throw std::logic_error("Encountered HE specific operation " +
                             getOpName(op_code) +
                             " in unencrypted computation");
    default:
      throw std::logic_error("Unhandled op " + getOpName(op_code));
    }
  }
};

} // namespace eva
