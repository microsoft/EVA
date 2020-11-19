// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include "eva/util/logging.h"
#include <stdexcept>
#include <vector>

namespace eva {

/*
Implements efficient forward and backward traversals of Program in the
presence of modifications during traversal.
The rewriter is called for each term in the Program exactly once.
Rewriters must not modify the Program in such a way that terms that are
not uses/operands (for forward/backward traversal, respectively) of the
current term are enabled. With such modifications the whole program is
not guaranteed to be traversed.
*/
class ProgramTraversal {
  Program &program;

  TermMap<bool> ready;
  TermMap<bool> processed;

  template <bool isForward> bool arePredecessorsDone(const Term::Ptr &term) {
    for (auto &operand : isForward ? term->getOperands() : term->getUses()) {
      if (!processed[operand]) return false;
    }
    return true;
  }

  template <typename Rewriter, bool isForward>
  void traverse(Rewriter &&rewrite) {
    processed.clear();
    ready.clear();

    std::vector<Term::Ptr> readyNodes =
        isForward ? program.getSources() : program.getSinks();
    for (auto &term : readyNodes) {
      ready[term] = true;
    }
    // Used for remembering uses/operands before rewrite is called. Using a
    // vector here is fine because duplicates in the list are handled
    // gracefully.
    std::vector<Term::Ptr> checkList;

    while (readyNodes.size() != 0) {
      // Pop term to transform
      auto term = readyNodes.back();
      readyNodes.pop_back();

      // If this term is removed, we will lose uses/operands of this term.
      // Remember them here for checking readyness after the rewrite.
      checkList.clear();
      for (auto &succ : isForward ? term->getUses() : term->getOperands()) {
        checkList.push_back(succ);
      }

      log(Verbosity::Trace, "Processing term with index=%lu", term->index);
      rewrite(term);
      processed[term] = true;

      // If transform adds new sources/sinks add them to ready terms.
      for (auto &leaf : isForward ? program.getSources() : program.getSinks()) {
        if (!ready[leaf]) {
          readyNodes.push_back(leaf);
          ready[leaf] = true;
        }
      }

      // Also check current uses/operands in case any new ones were added.
      for (auto &succ : isForward ? term->getUses() : term->getOperands()) {
        checkList.push_back(succ);
      }

      // Push and mark uses/operands that are ready to be processed.
      for (auto &succ : checkList) {
        if (!ready[succ] && arePredecessorsDone<isForward>(succ)) {
          readyNodes.push_back(succ);
          ready[succ] = true;
        }
      }
    }
  }

public:
  ProgramTraversal(Program &g) : program(g), processed(g), ready(g) {}

  template <typename Rewriter> void forwardPass(Rewriter &&rewrite) {
    traverse<Rewriter, true>(std::forward<Rewriter>(rewrite));
  }

  template <typename Rewriter> void backwardPass(Rewriter &&rewrite) {
    traverse<Rewriter, false>(std::forward<Rewriter>(rewrite));
  }
};

} // namespace eva
