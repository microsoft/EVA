// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class EagerRelinearizer {
  Program &program;
  TermMap<Type> &type;
  TermMapOptional<std::uint32_t> &scale;

  bool isMultiplicationOp(const Op &op_code) { return (op_code == Op::Mul); }

  bool isUnencryptedType(const Type &type) { return type != Type::Cipher; }

  bool areAllOperandsEncrypted(Term::Ptr &term) {
    for (auto &op : term->getOperands()) {
      if (isUnencryptedType(type[op])) {
        return false;
      }
    }
    return true;
  }

public:
  EagerRelinearizer(Program &g, TermMap<Type> &type,
                    TermMapOptional<std::uint32_t> &scale)
      : program(g), type(type), scale(scale) {}

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    auto &operands = term->getOperands();
    if (operands.size() == 0) return; // inputs

    auto op = term->op;

    if (!isMultiplicationOp(op)) return;

    // Op::Multiply only
    // ASSUME: only two operands
    bool encryptedOps = areAllOperandsEncrypted(term);
    if (!encryptedOps) return;

    auto relinNode = program.makeTerm(Op::Relinearize, {term});
    type[relinNode] = type[term];
    scale[relinNode] = scale[term];

    term->replaceOtherUsesWith(relinNode);
  }
};

} // namespace eva
