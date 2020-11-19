// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include <numeric>

namespace eva {

/*
This pass combines nodes to reduce the depth of the tree.
Suppose you have expression tree

    *
   / \
  *   t5(c)  =>           *
  /  \                 /  \  \
t1(c)  t2(c)        t1   t2   t5


Before combining first it checks if some node have only one use and both
of these nodes have same op then these two nodes are combined into one node
with children of both the nodes.
This pass helps to get the flat form of an expression so that later on it can
be expanded to get a expression in a balanced form.
For example (a * (b * (c * d))) => (a * b * c * d) => (a * b) * (c * d)
*/
class ReductionCombiner {
  Program &program;

  bool isReductionOp(const Op &op_code) {
    return ((op_code == Op::Add) || (op_code == Op::Mul));
  }

public:
  ReductionCombiner(Program &g) : program(g) {}

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    if (!term->isInternal() || !isReductionOp(term->op)) return;

    auto uses = term->getUses();
    if (uses.size() == 1) {
      auto &use = uses[0];
      if (use->op == term->op) {
        // combine term and its use
        while (use->eraseOperand(term)) {
          for (auto &operand : term->getOperands()) {
            // add term's operands to use's operands
            use->addOperand(operand);
          }
        }
      }
    }
  }
};

class ReductionLogExpander {
  Program &program;
  TermMap<Type> &type;
  TermMapOptional<int> scale;
  std::vector<Term::Ptr> operands, nextOperands;
  std::map<std::uint32_t, std::vector<Term::Ptr>> sortedOperands;

  bool isReductionOp(const Op &op_code) {
    return ((op_code == Op::Add) || (op_code == Op::Mul));
  }

public:
  ReductionLogExpander(Program &g, TermMap<Type> &type)
      : program(g), type(type), scale(g) {}

  void operator()(Term::Ptr &term) {
    if (term->op == Op::Rescale || term->op == Op::ModSwitch) {
      throw std::logic_error("Rescale or ModSwitch encountered, but "
                             "ReductionLogExpander uses scale as"
                             " a proxy for level and assumes rescaling has not "
                             "been performed yet.");
    }

    // Calculate the scales that we would get without any rescaling. Terms at a
    // similar scale will likely end up having the same level in typical
    // rescaling policies, which helps the sorting group terms of the same level
    // together.
    if (term->numOperands() == 0) {
      scale[term] = term->get<EncodeAtScaleAttribute>();
    } else if (term->op == Op::Mul) {
      scale[term] = std::accumulate(
          term->getOperands().begin(), term->getOperands().end(), 0,
          [&](auto &sum, auto &operand) { return sum + scale.at(operand); });
    } else {
      scale[term] = std::accumulate(term->getOperands().begin(),
                                    term->getOperands().end(), 0,
                                    [&](auto &max, auto &operand) {
                                      return std::max(max, scale.at(operand));
                                    });
    }

    if (isReductionOp(term->op) && term->numOperands() > 2) {
      // We sort operands into constants, plaintext and raw, then ciphertexts by
      // scale. This helps avoid unnecessary accumulation of scale.
      for (auto &operand : term->getOperands()) {
        auto order = 0;
        if (type[operand] == Type::Plain || type[operand] == Type::Raw) {
          order = 1;
        } else if (type[operand] == Type::Cipher) {
          order = 2 + scale.at(operand);
        }
        sortedOperands[order].push_back(operand);
      }
      for (auto &op : sortedOperands) {
        operands.insert(operands.end(), op.second.begin(), op.second.end());
      }

      // Expand the sorted operands into a balanced reduction tree by pairing
      // adjacent operands until only one remains.
      assert(operands.size() >= 2);
      while (operands.size() > 2) {
        std::size_t i = 0;
        while ((i + 1) < operands.size()) {
          auto &leftOperand = operands[i];
          auto &rightOperand = operands[i + 1];
          auto newTerm =
              program.makeTerm(term->op, {leftOperand, rightOperand});
          nextOperands.push_back(newTerm);
          i += 2;
        }
        if (i < operands.size()) {
          assert((i + 1) == operands.size());
          nextOperands.push_back(operands[i]);
        }
        operands = nextOperands;
        nextOperands.clear();
      }

      assert(operands.size() == 2);
      term->setOperands(operands);

      operands.clear();
      nextOperands.clear();
      sortedOperands.clear();
    }
  }
};

} // namespace eva
