// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ir/term.h"
#include "eva/ir/program.h"
#include <algorithm>
#include <utility>

using namespace std;

namespace eva {

Term::Term(Op op, Program &program)
    : op(op), program(program), index(program.allocateIndex()) {
  program.sources.insert(this);
  program.sinks.insert(this);
}

Term::~Term() {
  for (Ptr &operand : operands) {
    operand->eraseUse(this);
  }
  if (operands.empty()) {
    program.sources.erase(this);
  }
  assert(uses.empty());
  program.sinks.erase(this);
}

void Term::addOperand(const Term::Ptr &term) {
  if (operands.empty()) {
    program.sources.erase(this);
  }
  operands.emplace_back(term);
  term->addUse(this);
}

bool Term::eraseOperand(const Ptr &term) {
  auto iter = find(operands.begin(), operands.end(), term);
  if (iter != operands.end()) {
    term->eraseUse(this);
    operands.erase(iter);
    if (operands.empty()) {
      program.sources.insert(this);
    }
    return true;
  }
  return false;
}

bool Term::replaceOperand(Ptr oldTerm, Ptr newTerm) {
  bool replaced = false;
  for (Ptr &operand : operands) {
    if (operand == oldTerm) {
      operand = newTerm;
      oldTerm->eraseUse(this);
      newTerm->addUse(this);
      replaced = true;
    }
  }
  return replaced;
}

void Term::replaceUsesWithIf(Ptr term, function<bool(const Ptr &)> predicate) {
  auto thisPtr = shared_from_this(); // TODO: avoid this and similar
                                     // unnecessary reference counting
  for (auto &use : getUses()) {
    if (predicate(use)) {
      use->replaceOperand(thisPtr, term);
    }
  }
}

void Term::replaceAllUsesWith(Ptr term) {
  replaceUsesWithIf(term, [](const Ptr &) { return true; });
}

void Term::replaceOtherUsesWith(Ptr term) {
  replaceUsesWithIf(term, [&](const Ptr &use) { return use != term; });
}

void Term::setOperands(vector<Term::Ptr> o) {
  if (operands.empty()) {
    program.sources.erase(this);
  }

  for (auto &operand : operands) {
    operand->eraseUse(this);
  }
  operands = move(o);
  for (auto &operand : operands) {
    operand->addUse(this);
  }

  if (operands.empty()) {
    program.sources.insert(this);
  }
}

size_t Term::numOperands() const { return operands.size(); }

Term::Ptr Term::operandAt(size_t i) { return operands.at(i); }

const vector<Term::Ptr> &Term::getOperands() const { return operands; }

size_t Term::numUses() { return uses.size(); }

vector<Term::Ptr> Term::getUses() {
  vector<Term::Ptr> u;
  for (Term *use : uses) {
    u.emplace_back(use->shared_from_this());
  }
  return u;
}

bool Term::isInternal() const {
  return ((operands.size() != 0) && (uses.size() != 0));
}

void Term::addUse(Term *term) {
  if (uses.empty()) {
    program.sinks.erase(this);
  }
  uses.emplace_back(term);
}

bool Term::eraseUse(Term *term) {
  auto iter = find(uses.begin(), uses.end(), term);
  assert(iter != uses.end());
  uses.erase(iter);
  if (uses.empty()) {
    program.sinks.insert(this);
    return true;
  }
  return false;
}

ostream &operator<<(ostream &s, const Term &term) {
  s << term.index << ':' << getOpName(term.op) << '(';
  bool first = true;
  for (const auto &operand : term.getOperands()) {
    if (first) {
      first = false;
    } else {
      s << ',';
    }
    s << operand->index;
  }
  s << ')';
  return s;
}

} // namespace eva
