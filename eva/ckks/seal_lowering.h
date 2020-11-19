// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class SEALLowering {
  Program &program;
  TermMap<Type> &type;

public:
  SEALLowering(Program &g, TermMap<Type> &type) : program(g), type(type) {}

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal

    // SEAL does not support plaintext subtraction with a plaintext on the left
    // hand side, so lower to a negation and addition.
    if (term->op == Op::Sub && type[term->operandAt(0)] != Type::Cipher &&
        type[term->operandAt(1)] == Type::Cipher) {
      auto negation = program.makeTerm(Op::Negate, {term->operandAt(1)});
      auto addition = program.makeTerm(Op::Add, {term->operandAt(0), negation});
      term->replaceAllUsesWith(addition);
    }
  }
};

} // namespace eva
