// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ir/program.h"
#include "eva/common/program_traversal.h"
#include "eva/ir/term_map.h"
#include "eva/util/logging.h"
#include <stack>

using namespace std;

namespace eva {

// TODO: maybe replace with smart iterator to avoid allocation
vector<Term::Ptr> toTermPtrs(const unordered_set<Term *> &terms) {
  vector<Term::Ptr> termPtrs;
  termPtrs.reserve(terms.size());
  for (auto &term : terms) {
    termPtrs.emplace_back(term->shared_from_this());
  }
  return termPtrs;
}

vector<Term::Ptr> Program::getSources() const {
  return toTermPtrs(this->sources);
}

vector<Term::Ptr> Program::getSinks() const { return toTermPtrs(this->sinks); }

std::unique_ptr<Program> Program::deepCopy() {
  auto newProg = std::make_unique<Program>(getName(), getVecSize());
  TermMap<Term::Ptr> oldToNew(*this);
  ProgramTraversal traversal(*this);
  traversal.forwardPass([&](Term::Ptr &term) {
    auto newTerm = newProg->makeTerm(term->op);
    oldToNew[term] = newTerm;
    newTerm->assignAttributesFrom(*term);
    for (auto &operand : term->getOperands()) {
      newTerm->addOperand(oldToNew[operand]);
    }
  });
  for (auto &entry : inputs) {
    newProg->inputs[entry.first] = oldToNew[entry.second];
  }
  for (auto &entry : outputs) {
    newProg->outputs[entry.first] = oldToNew[entry.second];
  }
  return newProg;
}

uint64_t Program::allocateIndex() {
  // TODO: reuse released indices to save space in TermMap instances
  uint64_t index = nextTermIndex++;
  for (TermMapBase *termMap : termMaps) {
    termMap->resize(nextTermIndex);
  }
  return index;
}

void Program::initTermMap(TermMapBase &termMap) {
  termMap.resize(nextTermIndex);
}

void Program::registerTermMap(TermMapBase *termMap) {
  termMaps.emplace_back(termMap);
}

void Program::unregisterTermMap(TermMapBase *termMap) {
  auto iter = find(termMaps.begin(), termMaps.end(), termMap);
  if (iter == termMaps.end()) {
    throw runtime_error("TermMap to unregister not found");
  } else {
    termMaps.erase(iter);
  }
}

template <class Attr>
void dumpAttribute(stringstream &s, Term *term, std::string label) {
  if (term->has<Attr>()) {
    s << ", " << label << "=" << term->get<Attr>();
  }
}

// Print an attribute in DOT format as a box outside the term
template <class Attr>
void toDOTAttributeAsNode(stringstream &s, Term *term, std::string label) {
  if (term->has<Attr>()) {
    s << "t" << term->index << "_" << getAttributeName(Attr::key)
      << " [shape=box label=\"" << label << "=" << term->get<Attr>()
      << "\"];\n";
    s << "t" << term->index << "_" << getAttributeName(Attr::key) << " -> t"
      << term->index << ";\n";
  }
}

string Program::dump(TermMapOptional<std::uint32_t> &scales,
                     TermMap<eva::Type> &types,
                     TermMap<std::uint32_t> &level) const {
  // TODO: switch to use a non-parallel generic traversal
  stringstream s;
  s << getName() << "(){\n";

  // Add all terms in topologically sorted order
  uint64_t nextIndex = 0;
  unordered_map<Term *, uint64_t> indices;
  stack<pair<bool, Term *>> work;
  for (const auto &sink : getSinks()) {
    work.emplace(true, sink.get());
  }
  while (!work.empty()) {
    bool visit = work.top().first;
    auto term = work.top().second;
    work.pop();
    if (indices.count(term)) {
      continue;
    }
    if (visit) {
      work.emplace(false, term);
      for (const auto &operand : term->getOperands()) {
        work.emplace(true, operand.get());
      }
    } else {
      auto index = nextIndex;
      nextIndex += 1;
      indices[term] = index;
      s << "t" << term->index << " = " << getOpName(term->op);
      if (term->has<RescaleDivisorAttribute>()) {
        s << "(" << term->get<RescaleDivisorAttribute>() << ")";
      }
      if (term->has<RotationAttribute>()) {
        s << "(" << term->get<RotationAttribute>() << ")";
      }
      if (term->has<TypeAttribute>()) {
        s << ":" << getTypeName(term->get<TypeAttribute>());
      }
      for (int i = 0; i < term->numOperands(); ++i) {
        s << " t" << term->operandAt(i)->index;
      }
      dumpAttribute<RangeAttribute>(s, term, "range");
      dumpAttribute<EncodeAtLevelAttribute>(s, term, "level");
      if (types[*term] == Type::Cipher)
        s << ", "
          << "s"
          << "=" << scales[*term] << ", t=cipher ";
      else if (types[*term] == Type::Raw)
        s << ", "
          << "s"
          << "=" << scales[*term] << ", t=raw ";
      else
        s << ", "
          << "s"
          << "=" << scales[*term] << ", t=plain ";
      s << "\n";
      // ConstantValue TODO: printing constant values for simple cases
    }
  }

  s << "}\n";
  return s.str();
}

string Program::toDOT() const {
  // TODO: switch to use a non-parallel generic traversal
  stringstream s;

  s << "digraph \"" << getName() << "\" {\n";

  // Add all terms in topologically sorted order
  uint64_t nextIndex = 0;
  unordered_map<Term *, uint64_t> indices;
  stack<pair<bool, Term *>> work;
  for (const auto &sink : getSinks()) {
    work.emplace(true, sink.get());
  }
  while (!work.empty()) {
    bool visit = work.top().first;
    auto term = work.top().second;
    work.pop();
    if (indices.count(term)) {
      continue;
    }
    if (visit) {
      work.emplace(false, term);
      for (const auto &operand : term->getOperands()) {
        work.emplace(true, operand.get());
      }
    } else {
      auto index = nextIndex;
      nextIndex += 1;
      indices[term] = index;

      // Operands are guaranteed to have been added
      s << "t" << term->index << " [label=\"" << getOpName(term->op);
      if (term->has<RescaleDivisorAttribute>()) {
        s << "(" << term->get<RescaleDivisorAttribute>() << ")";
      }
      if (term->has<RotationAttribute>()) {
        s << "(" << term->get<RotationAttribute>() << ")";
      }
      if (term->has<TypeAttribute>()) {
        s << " : " << getTypeName(term->get<TypeAttribute>());
      }
      s << "\""; // End label
      s << "];\n";
      for (int i = 0; i < term->numOperands(); ++i) {
        s << "t" << term->operandAt(i)->index << " -> t" << term->index
          << " [label=\"" << i << "\"];\n";
      }
      toDOTAttributeAsNode<RangeAttribute>(s, term, "range");
      toDOTAttributeAsNode<EncodeAtScaleAttribute>(s, term, "scale");
      toDOTAttributeAsNode<EncodeAtLevelAttribute>(s, term, "level");
      // ConstantValue TODO: printing constant values for simple cases
    }
  }

  s << "}\n";

  return s.str();
}

} // namespace eva
