// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class TypeDeducer {
  Program &program;
  TermMap<Type> &types;

public:
  TypeDeducer(Program &g, TermMap<Type> &types) : program(g), types(types) {}

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    auto &operands = term->getOperands();
    if (operands.size() > 0) {   // not an input/root
      Type inferred = Type::Raw; // Plain if not Cipher
      for (auto &operand : operands) {
        if (types[operand] == Type::Cipher)
          inferred = Type::Cipher; // Cipher if any operand is Cipher
      }
      if (term->op == Op::Encode) {
        types[term] = Type::Plain;
      } else {
        types[term] = inferred;
      }
    } else if (term->op == Op::Constant) {
      types[term] = Type::Raw;
    } else {
      types[term] = term->get<TypeAttribute>();
    }
  }
};

} // namespace eva
