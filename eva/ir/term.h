// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/attributes.h"
#include "eva/ir/ops.h"
#include "eva/ir/types.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <unordered_set>
#include <vector>

namespace eva {

class Program;

class Term : public AttributeList, public std::enable_shared_from_this<Term> {
public:
  using Ptr = std::shared_ptr<Term>;

  Term(Op opcode, Program &program);
  ~Term();

  void addOperand(const Ptr &term);
  bool eraseOperand(const Ptr &term);
  bool replaceOperand(Ptr oldTerm, Ptr newTerm);
  void setOperands(std::vector<Ptr> o);
  std::size_t numOperands() const;
  Ptr operandAt(size_t i);
  const std::vector<Ptr> &getOperands() const;

  void replaceUsesWithIf(Ptr term, std::function<bool(const Ptr &)>);
  void replaceAllUsesWith(Ptr term);
  void replaceOtherUsesWith(Ptr term);

  std::size_t numUses();
  std::vector<Ptr> getUses();

  bool isInternal() const;

  const Op op;
  Program &program;

  // Unique index for this Term in the owning Program. Managed by Program
  // and used to index into TermMap instances.
  std::uint64_t index;

  friend std::ostream &operator<<(std::ostream &s, const Term &term);

private:
  std::vector<Ptr> operands; // use->def chain (unmanaged pointers)
  std::vector<Term *> uses;  // def->use chain (managed pointers)

  void addUse(Term *term);
  bool eraseUse(Term *term);
};

} // namespace eva
