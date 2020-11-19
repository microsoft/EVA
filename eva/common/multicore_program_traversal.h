// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include "eva/util/galois.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <galois/substrate/PerThreadStorage.h>
#include <set>
#include <vector>

namespace eva {

class MulticoreProgramTraversal {
public:
  MulticoreProgramTraversal(Program &g) : program_(g) {}

  template <typename Evaluator> void forwardPass(Evaluator &eval) {
    TermMap<std::atomic_uint32_t> predecessors(program_);
    TermMap<std::atomic_uint32_t> successors(program_);

    // Add the source terms
    galois::InsertBag<Term::Ptr> readyNodes;
    for (auto source : program_.getSources()) {
      readyNodes.push_back(source);
    }

    // Enumerate predecessors and successors
    galois::for_each(
        galois::iterate(readyNodes),
        [&](const Term::Ptr &term, auto &ctx) {
          // For each term, iterate over its uses
          for (auto &use : term->getUses()) {
            // Increment the number of successors
            ++successors[term];

            // Increment the number of predecessors
            if ((++predecessors[use]) == 1) {
              // Only first predecessor will push so each use is added once
              ctx.push_back(use);
            }
          }
        },
        galois::wl<galois::worklists::PerSocketChunkFIFO<1>>(),
        galois::no_stats(),
        galois::loopname("ForwardCountPredecessorsSuccessors"));

    // Traverse the program
    galois::for_each(
        galois::iterate(readyNodes),
        [&](const Term::Ptr &term, auto &ctx) {
          // Process the current term
          eval(term);

          // Free operands if their successors are done
          for (auto &operand : term->getOperands()) {
            if ((--successors[operand]) == 0) {
              // Only last successor will free
              eval.free(operand);
            }
          }

          // Execute (ready) uses if their predecessors are done
          for (auto &use : term->getUses()) {
            if ((--predecessors[use]) == 0) {
              // Only last predecessor will push
              ctx.push_back(use);
            }
          }
        },
        galois::wl<galois::worklists::PerSocketChunkFIFO<1>>(),
        galois::no_stats(), galois::loopname("ForwardTraversal"));

    // TODO: Reinstate these checks
    // for (auto& predecessor : predecessors) assert(predecessor == 0);
    // for (auto& successor : successors) assert(successor == 0);
  }

  template <typename Evaluator> void backwardPass(Evaluator &eval) {
    TermMap<std::atomic_uint32_t> predecessors(program_);
    TermMap<std::atomic_uint32_t> successors(program_);

    // Add the sink terms
    galois::InsertBag<Term::Ptr> readyNodes;
    for (auto &sink : program_.getSinks()) {
      readyNodes.push_back(sink);
    }

    // Enumerate predecessors and successors
    galois::for_each(
        galois::iterate(readyNodes),
        [&](const Term::Ptr &term, auto &ctx) {
          // For each term, iterate over its operands
          for (auto &operand : term->getOperands()) {
            // Increment the number of predecessors
            ++predecessors[term];

            // Increment the number of successors for the operand
            if ((++successors[operand]) == 1) {
              // Only first successor will push so each operand is added once
              ctx.push_back(operand);
            }
          }
        },
        galois::wl<galois::worklists::PerSocketChunkFIFO<1>>(),
        galois::no_stats(),
        galois::loopname("BackwardCountPredecessorsSuccessors"));

    // Traverse the program
    galois::for_each(
        galois::iterate(readyNodes),
        [&](const Term::Ptr &term, auto &ctx) {
          // Process the current term
          eval(term);

          // Free uses if their predecessors are done
          for (auto &use : term->getUses()) {
            if ((--predecessors[use]) == 0) {
              // Only last predecessor will free
              eval.free(use);
            }
          }

          // Execute (ready) operands if their successors are done
          for (auto &operand : term->getOperands()) {
            if ((--successors[operand]) == 0) {
              // Only last successor will push
              ctx.push_back(operand);
            }
          }
        },
        galois::wl<galois::worklists::PerSocketChunkFIFO<1>>(),
        galois::no_stats(), galois::loopname("BackwardTraversal"));

    // TODO: Reinstate these checks
    // for (auto& predecessor : predecessors) assert(predecessor == 0);
    // for (auto& successor : successors) assert(successor == 0);
  }

private:
  Program &program_;
  GaloisGuard galoisGuard_;
};

} // namespace eva
