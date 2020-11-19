// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/rescaler.h"
#include "eva/util/logging.h"

namespace eva {

class LazyWaterlineRescaler : public Rescaler {
  std::uint32_t minScale;
  const std::uint32_t fixedRescale = 60;
  TermMap<bool> pending; // maintains whether rescaling is pending
  // TODO: level is no longer used. Should be removed.
  TermMap<std::uint32_t> level; // maintains the number of rescalings (levels)

  void insertRescaleRecursive(Term::Ptr &term) {
    auto temp = term;
    auto termScale = scale[temp];
    std::size_t num = 0;
    while (termScale >= (fixedRescale + minScale)) {
      temp = insertRescale(temp, fixedRescale);
      ++num;
      termScale -= fixedRescale;
      assert(termScale == scale[temp]);
    }
    level[temp] = level[term] + num;
  }

public:
  LazyWaterlineRescaler(Program &g, TermMap<Type> &type,
                        TermMapOptional<std::uint32_t> &scale)
      : Rescaler(g, type, scale), pending(g), level(g) {
    // ASSUME: minScale is max among all the inputs' scale
    minScale = 0;
    for (auto &source : program.getSources()) {
      if (scale[source] > minScale) minScale = scale[source];
    }
    assert(minScale != 0);
  }

  void
  operator()(Term::Ptr &term) { // must only be used with forward pass traversal
    if (term->numOperands() == 0) return; // inputs
    if (type[term] == Type::Raw) {
      handleRawScale(term);
      return;
    }

    auto op = term->op;

    bool delayed = false;

    if (isRescaleOp(op)) {
      return; // already processed
    } else if (isMultiplicationOp(op)) {
      assert(pending[term] == false);

      std::uint32_t multScale = 0;
      std::uint32_t maxLevel = 0;
      for (auto &operand : term->getOperands()) {
        multScale += scale[operand];
        if (level[operand] > maxLevel) maxLevel = level[operand];

        // The following assertion does not currently hold, as the
        // multiplications added for matching addition operand scales can
        // sometimes scale up a pending term.
        // assert(pending[operand] == false);
      }
      assert(multScale != 0);
      scale[term] = multScale;
      level[term] = maxLevel;

      // rescale only if above the waterline
      auto temp = term;
      if (multScale >= (fixedRescale + minScale)) {
        pending[term] = true;
        delayed = true;
      } else {
        return;
      }
    } else {
      // Op::Add, Op::Sub, NEGATE, COPY, Op::RotateLeftConst,
      // Op::RotateRightConst copy scale of the first operand
      scale[term] = scale[term->operandAt(0)];
      level[term] = level[term->operandAt(0)];
      if (isAdditionOp(op)) {
        // Op::Add, Op::Sub
        std::uint32_t maxLevel = 0;
        for (auto &operand : term->getOperands()) {
          if (level[operand] > maxLevel) maxLevel = level[operand];
        }
        level[term] = maxLevel;

        auto maxScale = scale[term];
        for (auto &operand : term->getOperands()) {
          if (scale[operand] > maxScale) maxScale = scale[operand];
        }
        scale[term] = maxScale;

        // ensure that all operands have same scale
        for (auto &operand : term->getOperands()) {
          if (scale[operand] < maxScale && type[operand] != Type::Raw) {
            log(Verbosity::Trace,
                "Scaling up t%i from scale %i to match other addition operands "
                "at scale %i",
                operand->index, scale[operand], maxScale);

            auto scaleConstant = program.makeUniformConstant(1);
            scale[scaleConstant] = maxScale - scale[operand];
            scaleConstant->set<EncodeAtScaleAttribute>(scale[scaleConstant]);

            auto mulNode = program.makeTerm(Op::Mul, {operand, scaleConstant});
            scale[mulNode] = maxScale;

            term->replaceOperand(operand, mulNode);
          }
        }
        // assert that all operands have the same scale
        for (auto &operand : term->getOperands()) {
          assert(maxScale == scale[operand] || type[operand] == Type::Raw);
        }
      }

      if (pending[term] == false) {
        return;
      }
    }

    assert(pending[term] == true);

    bool mustInsert = false;
    assert(term->numUses() > 0);
    auto firstUse = term->getUses()[0];
    for (auto &use : term->getUses()) {
      if (isMultiplicationOp(use->op) || use->op == Op::Output ||
          (firstUse != use)) { // different uses
        mustInsert = true;
        break;
      }
    }

    if (mustInsert) {
      pending[term] = false;
      insertRescaleRecursive(term);
    } else {
      for (auto &use : term->getUses()) {
        pending[use] = true;
      }
    }
  }
};

// This is a legacy rescaler from before the reduction balancing had support
// for estimating operand levels.
/*
class LazyWaterlineRescalerReductionLogExpander : public Rescaler {
  std::uint32_t minScale;
  const std::uint32_t fixedRescale = 60;
  TermMap<bool> pending;        // maintains whether rescaling is pending
  TermMap<std::uint32_t> level; // maintains the number of rescalings (levels)
  std::uint32_t count;
  std::uint32_t countTotal;

  bool isPlainType(const Type &type) const { return type == Type::Plain; }
  bool isRawType(const Type &type) const { return type == Type::Raw; }

  bool isCipherType(const Type &type) const { return type == Type::Cipher; }

  Term::Ptr expand(Term::Ptr &leftOperand, Term::Ptr &rightOperand,
                   Term::Ptr &use) {
    // create new operand
    auto newOperand = program.makeTerm(use->op, {leftOperand, rightOperand});
                if(isCipherType(type[leftOperand]) ||
isCipherType(type[rightOperand])){ type[newOperand] = Type::Cipher;
                }
                else if(isPlainType(type[leftOperand]) ||
isPlainType(type[rightOperand])){ type[newOperand] = Type::Plain;
                }
                else{
                        type[newOperand] = Type::Raw;
                }
    return newOperand;
  }

  auto insertRescaleRecursive(Term::Ptr &term) {
    auto temp = term;
    auto termScale = scale[temp];
    std::size_t num = 0;
    while (termScale >= (fixedRescale + minScale)) {
      temp = insertRescale(temp, fixedRescale);
      ++num;
      termScale -= fixedRescale;
      assert(termScale == scale[temp]);
    }
    level[temp] = level[term] + num;
    countTotal += num;
    return temp;
  }

  void expandRecursive(Term::Ptr &term) {
    std::vector<Term::Ptr> operands;
    std::map<std::uint32_t, std::vector<Term::Ptr>> sortedOperands;
    for (auto &operand : term->getOperands()) {
      auto operandLevel = level[operand];
      if (isCipherType(type[operand])) {
        operandLevel += 2;
      } else {
        if (isPlainType(type[operand])) {
          operandLevel = 1;
        }
      }
      sortedOperands[operandLevel].push_back(operand);
    }

    // SORTED ORDER: constants, plaintext, then ciphertext ordered by level
    for (auto &op : sortedOperands) {
      operands.insert(operands.end(), op.second.begin(), op.second.end());
    }

    bool isMultOp = isMultiplicationOp(term->op);

    std::vector<Term::Ptr> nextOperands;
    assert(operands.size() >= 2);
    while (operands.size() > 2) {
      std::size_t i = 0;
      // TODO: fix this to include levels
      while ((i + 1) < operands.size()) {
        auto &leftOperand = operands[i];
        auto &rightOperand = operands[i + 1];
        auto newOperand = expand(leftOperand, rightOperand, term);
        if (isMultOp) {
          processMultiplicationOp(newOperand);
          if (scale[newOperand] >= (fixedRescale + minScale)) {
            newOperand = insertRescaleRecursive(newOperand);
          }
        } else {
          processAdditionOp(newOperand);
        }
        nextOperands.push_back(newOperand);
        i += 2;
      }
      if (i < operands.size()) {
        assert((i + 1) == operands.size());
        nextOperands.push_back(operands[i]);
      }
      operands = nextOperands;
      nextOperands.clear();
    }

    // clear term's operands and set it to the current operands
    assert(operands.size() == 2);
    term->setOperands({operands[0], operands[1]});
  }

  void processMultiplicationOp(Term::Ptr &term) {
                if(type[term] == Type::Raw){
        scale[term] = fixedRescale;
                        return;
                }
    auto &operands = term->getOperands();

    std::uint32_t multScale = 0;
    std::uint32_t maxLevel = 0;
    for (auto &operand : operands) {
      multScale += scale[operand];
      if (level[operand] > maxLevel) maxLevel = level[operand];
      assert(pending[operand] == false);
      assert(scale[operand] < (fixedRescale + minScale));
    }
    assert(multScale != 0);
    scale[term] = multScale;
    level[term] = maxLevel;
  }

  void processAdditionOp(Term::Ptr &term) {
    auto &operands = term->getOperands();

    std::uint32_t maxLevel = 0;
    for (auto &operand : operands) {
      if (level[operand] > maxLevel) maxLevel = level[operand];
    }
    level[term] = maxLevel;

    auto maxScale = 0;
    for (auto &operand : operands) {
      if (scale[operand] > maxScale) maxScale = scale[operand];
    }
    scale[term] = maxScale;

    // assert that pending (delaying rescaling) does not increase level
    // TODO: this is insufficient
    std::uint32_t maxLevel2 = 0;
    for (auto &operand : operands) {
      auto operandScale = scale[operand];
      auto operandLevel = level[operand];
      while (operandScale >= (fixedRescale + minScale)) {
        operandScale -= fixedRescale;
        ++operandLevel;
      }
      if (operandLevel > maxLevel2) maxLevel2 = operandLevel;
    }
    {
      auto scale2 = maxScale;
      auto level2 = maxLevel;
      while (scale2 >= (fixedRescale + minScale)) {
        scale2 -= fixedRescale;
        ++level2;
      }
      assert(level2 <= maxLevel2);
    }

    // ensure that all operands have same scale
    for (auto &operand : operands) {
      if (scale[operand] < maxScale) {
        auto scaleConstant = program.makeUniformConstant(1);
        scale[scaleConstant] = maxScale - scale[operand];
        scaleConstant->set<EncodeAtScaleAttribute>(scale[scaleConstant]);
        auto mulNode = program.makeTerm(Op::Mul, {operand, scaleConstant});
        scale[mulNode] = maxScale;

        // TODO: Not obviously correct as it's modifying inside
        // iteration. Refine API to make this less surprising.
        term->replaceOperand(operand, mulNode);
      }
    }
    // assert that all operands have the same scale
    for (auto &operand : operands) {
      assert(maxScale == scale[operand]);
    }
  }

public:
  LazyWaterlineRescalerReductionLogExpander(
      Program &g, TermMap<Type> &type, TermMapOptional<std::uint32_t> &scale)
      : Rescaler(g, type, scale), pending(g), level(g) {
    // ASSUME: minScale is max among all the inputs' scale
    minScale = 0;
    for (auto &source : program.getSources()) {
      if (scale[source] > minScale) minScale = scale[source];
    }
    assert(minScale != 0);
    count = 0;
    countTotal = 0;
  }

  ~LazyWaterlineRescalerReductionLogExpander() {
    // TODO: move these to a logging system
    // std::cout << "Number of delayed rescales: " << count << "\n";
    // std::cout << "Number of rescales: " << countTotal << "\n";
  }

  // Must only be used with forward pass traversal
  void operator()(Term::Ptr &term) {
    auto &operands = term->getOperands();
    if (operands.size() == 0) return; // inputs

    auto op = term->op;

    bool delayed = false;

    if (isRescaleOp(op)) {
      return; // already processed
    } else if (isMultiplicationOp(op)) {
      assert(pending[term] == false);
      if (operands.size() > 2) {
        expandRecursive(term);
      }

      processMultiplicationOp(term);

      // rescale only if above the waterline
      auto temp = term;
      if (scale[term] >= (fixedRescale + minScale)) {
        pending[term] = true;
        delayed = true;
      } else {
        return;
      }
    } else {
      // Op::Add, Op::Sub, NEGATE, COPY, Op::RotateLeftConst,
      // Op::RotateRightConst
      if (isAdditionOp(op)) { // Op::Add, Op::Sub
        if ((op == Op::Add) && (operands.size() > 2)) {
          expandRecursive(term);
        }
        processAdditionOp(term);
      } else {
        // copy scale of the first operand
        for (auto &operand : operands) {
          scale[term] = scale[operand];
          level[term] = level[operand];
          break;
        }
      }

      if (pending[term] == false) {
        return;
      }
    }

    // assert(pending[term] == true);

    bool mustInsert = false;
    if (term->numUses() > 0) {
      auto firstUse = term->getUses()[0];
      for (auto &use : term->getUses()) {
        if (isMultiplicationOp(use->op) ||
            use->op == Op::Output ||
            (firstUse != use)) { // different uses
          mustInsert = true;
          break;
        }
      }
    } else {
      assert(term->op == Op::Output);
    }

    if (mustInsert) {
      pending[term] = false;
      insertRescaleRecursive(term);
    } else {
      if (delayed) ++count;
      for (auto &use : term->getUses()) {
        pending[use] = true;
      }
    }
  }
};
*/

} // namespace eva
