// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class EncodeInserter {
  Program &program;
  TermMap<Type> &type;
  TermMapOptional<std::uint32_t> &scale;

  bool isRawType(const Type &type) { return type == Type::Raw; }
  bool isCipherType(const Type &type) { return type == Type::Cipher; }
  bool isAdditionOp(const Op &op_code) {
    return ((op_code == Op::Add) || (op_code == Op::Sub));
  }

  auto insertEncodeNode(Op op, const Term::Ptr &other, const Term::Ptr &term) {
    auto newNode = program.makeTerm(Op::Encode, {term});
    type[newNode] = Type::Plain;
    if (isAdditionOp(op)) {
      scale[newNode] = scale[other];
    } else {
      scale[newNode] = scale[term];
    }
    newNode->set<EncodeAtScaleAttribute>(scale[newNode]);
    return newNode;
  }

public:
  EncodeInserter(Program &g, TermMap<Type> &type,
                 TermMapOptional<std::uint32_t> &scale)
      : program(g), type(type), scale(scale) {}

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    auto &operands = term->getOperands();
    if (operands.size() == 0) return; // inputs

    assert(operands.size() <= 2);
    if (operands.size() == 2) {
      auto &leftOperand = operands[0];
      auto &rightOperand = operands[1];
      auto op1 = leftOperand->op;
      if (isCipherType(type[leftOperand]) && isRawType(type[rightOperand])) {
        auto newTerm = insertEncodeNode(term->op, leftOperand, rightOperand);
        term->replaceOperand(rightOperand, newTerm);
      }

      if (isCipherType(type[rightOperand]) && isRawType(type[leftOperand])) {
        auto newTerm = insertEncodeNode(term->op, rightOperand, leftOperand);
        term->replaceOperand(leftOperand, newTerm);
      }
    }
  }
};

} // namespace eva
