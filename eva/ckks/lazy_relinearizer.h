// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class LazyRelinearizer {
  Program &program;
  TermMap<Type> &type;
  TermMapOptional<std::uint32_t> &scale;
  TermMap<bool> pending; // maintains whether relinearization is pending
  std::uint32_t count;
  std::uint32_t countTotal;

  bool isMultiplicationOp(const Op &op_code) { return (op_code == Op::Mul); }

  bool isRotationOp(const Op &op_code) {
    return ((op_code == Op::RotateLeftConst) ||
            (op_code == Op::RotateRightConst));
  }

  bool isUnencryptedType(const Type &type) { return type != Type::Cipher; }

  bool areAllOperandsEncrypted(Term::Ptr &term) {
    for (auto &op : term->getOperands()) {
      assert(type[op] != Type::Undef);
      if (isUnencryptedType(type[op])) {
        return false;
      }
    }
    return true;
  }

  bool isEncryptedMultOp(Term::Ptr &term) {
    return (isMultiplicationOp(term->op) && areAllOperandsEncrypted(term));
  }

public:
  LazyRelinearizer(Program &g, TermMap<Type> &type,
                   TermMapOptional<std::uint32_t> &scale)
      : program(g), type(type), scale(scale), pending(g) {
    count = 0;
    countTotal = 0;
  }

  ~LazyRelinearizer() {
    // TODO: move these to a logging system
    // std::cout << "Number of delayed relin: " << count << "\n";
    // std::cout << "Number of relin: " << countTotal << "\n";
  }

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    auto &operands = term->getOperands();
    if (operands.size() == 0) return; // inputs

    bool delayed = false;

    if (isEncryptedMultOp(term)) {
      assert(pending[term] == false);
      pending[term] = true;
      delayed = true;
    } else if (pending[term] == false) {
      return;
    }

    bool mustInsert = false;
    assert(term->numUses() > 0);
    auto firstUse = term->getUses()[0];
    for (auto &use : term->getUses()) {
      if (isEncryptedMultOp(use) || isRotationOp(use->op) ||
          use->op == Op::Output || (firstUse != use)) { // different uses
        mustInsert = true;
        break;
      }
    }

    if (mustInsert) {
      auto relinNode = program.makeTerm(Op::Relinearize, {term});
      ++countTotal;

      type[relinNode] = type[term];
      scale[relinNode] = scale[term];
      term->replaceOtherUsesWith(relinNode);
    } else {
      if (delayed) ++count;
      for (auto &use : term->getUses()) {
        pending[use] = true;
      }
    }
  }
};

} // namespace eva
