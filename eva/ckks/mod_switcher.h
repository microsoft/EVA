// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term_map.h"

namespace eva {

class ModSwitcher {
  Program &program;
  TermMap<Type> &type;
  TermMapOptional<std::uint32_t> &scale;
  TermMap<std::uint32_t>
      level; // maintains the reverse level (leaves have 0, roots have max)
  std::vector<Term::Ptr> encodeNodes;

  Term::Ptr insertModSwitchNode(Term::Ptr &term, std::uint32_t termLevel) {
    auto newNode = program.makeTerm(Op::ModSwitch, {term});
    scale[newNode] = scale[term];
    level[newNode] = termLevel;
    return newNode;
  }

  bool isRescaleOp(const Op &op_code) { return (op_code == Op::Rescale); }

  bool isCipherType(const Term::Ptr &term) const {
    return type[term] == Type::Cipher;
  }

public:
  ModSwitcher(Program &g, TermMap<Type> &type,
              TermMapOptional<std::uint32_t> &scale)
      : program(g), type(type), scale(scale), level(g) {}

  ~ModSwitcher() {
    auto sources = program.getSources();
    std::uint32_t maxLevel = 0;
    for (auto &source : sources) {
      if (level[source] > maxLevel) maxLevel = level[source];
    }
    for (auto &source : sources) {
      auto curLevel = maxLevel - level[source];
      source->set<EncodeAtLevelAttribute>(curLevel);
    }

    for (auto &encode : encodeNodes) {
      encode->set<EncodeAtLevelAttribute>(maxLevel - level[encode]);
    }
  }

  void operator()(
      Term::Ptr &term) { // must only be used with backward pass traversal
    if (term->numUses() == 0) return;

    //we do not want to add modswitch for nodes of type raw
    if (type[term] == Type::Raw) return;

    if (term->op == Op::Encode) {
      encodeNodes.push_back(term);
    }
    std::map<std::uint32_t, std::vector<Term::Ptr>> useLevels; // ordered map
    for (auto &use : term->getUses()) {
      useLevels[level[use]].push_back(use);
    }

    std::uint32_t termLevel = 0;
    if (useLevels.size() > 1) {
      auto useLevel = useLevels.rbegin(); // max to min
      termLevel = useLevel->first;
      ++useLevel;

      auto temp = term;
      auto tempLevel = termLevel;
      while (useLevel != useLevels.rend()) {
        auto expectedLevel = useLevel->first;
        while (tempLevel > expectedLevel) {
          temp = insertModSwitchNode(temp, tempLevel);
          --tempLevel;
        }
        for (auto &use : useLevel->second) {
          use->replaceOperand(term, temp);
        }
        ++useLevel;
      }
    } else {
      assert(useLevels.size() == 1);
      termLevel = useLevels.begin()->first;
    }
    if (isRescaleOp(term->op)) {
      ++termLevel;
    }
    level[term] = termLevel;
  }
};

} // namespace eva
