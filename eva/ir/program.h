// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/constant_value.h"
#include "eva/ir/term.h"
#include "eva/serialization/eva.pb.h"
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eva {

template <typename> class TermMapOptional;
template <typename> class TermMap;
class TermMapBase;

class Program {
public:
  Program(std::string name, std::uint64_t vecSize)
      : name(name), vecSize(vecSize), nextTermIndex(0) {
    if (vecSize == 0) {
      throw std::runtime_error("Vector size must be non-zero");
    }
    if ((vecSize & (vecSize - 1)) != 0) {
      throw std::runtime_error("Vector size must be a power-of-two");
    }
  }

  Program(const Program &copy) = delete;

  Program &operator=(const Program &assign) = delete;

  Term::Ptr makeTerm(Op op, const std::vector<Term::Ptr> &operands = {}) {
    auto term = std::make_shared<Term>(op, *this);
    if (operands.size() > 0) {
      term->setOperands(operands);
    }
    return term;
  }

  Term::Ptr makeConstant(std::unique_ptr<ConstantValue> value) {
    auto term = makeTerm(Op::Constant);
    term->set<ConstantValueAttribute>(std::move(value));
    return term;
  }

  Term::Ptr makeDenseConstant(std::vector<double> values) {
    return makeConstant(std::make_unique<DenseConstantValue>(vecSize, values));
  }

  Term::Ptr makeUniformConstant(double value) {
    return makeDenseConstant({value});
  }

  Term::Ptr makeInput(const std::string &name, Type type = Type::Cipher) {
    auto term = makeTerm(Op::Input);
    term->set<TypeAttribute>(type);
    inputs.emplace(name, term);
    return term;
  }

  Term::Ptr makeOutput(std::string name, const Term::Ptr &term) {
    auto output = makeTerm(Op::Output, {term});
    outputs.emplace(name, output);
    return output;
  }

  Term::Ptr makeLeftRotation(const Term::Ptr &term, std::int32_t slots) {
    auto rotation = makeTerm(Op::RotateLeftConst, {term});
    rotation->set<RotationAttribute>(slots);
    return rotation;
  }

  Term::Ptr makeRightRotation(const Term::Ptr &term, std::int32_t slots) {
    auto rotation = makeTerm(Op::RotateRightConst, {term});
    rotation->set<RotationAttribute>(slots);
    return rotation;
  }

  Term::Ptr makeRescale(const Term::Ptr &term, std::uint32_t rescaleBy) {
    auto rescale = makeTerm(Op::Rescale, {term});
    rescale->set<RescaleDivisorAttribute>(rescaleBy);
    return rescale;
  }

  Term::Ptr getInput(std::string name) const {
    if (inputs.find(name) == inputs.end()) {
      std::stringstream s;
      s << "No input named " << name;
      throw std::out_of_range(s.str());
    }
    return inputs.at(name);
  }

  const auto &getInputs() const { return inputs; }

  const auto &getOutputs() const { return outputs; }

  std::string getName() const { return name; }
  void setName(std::string newName) { name = newName; }

  std::uint32_t getVecSize() const { return vecSize; }

  std::vector<Term::Ptr> getSources() const;

  std::vector<Term::Ptr> getSinks() const;

  // Make a deep copy of this program
  std::unique_ptr<Program> deepCopy();

  std::string toDOT() const;
  std::string dump(TermMapOptional<std::uint32_t> &scales,
                   TermMap<eva::Type> &types,
                   TermMap<std::uint32_t> &level) const;

private:
  std::uint64_t allocateIndex();
  void initTermMap(TermMapBase &termMap);
  void registerTermMap(TermMapBase *annotation);
  void unregisterTermMap(TermMapBase *annotation);

  std::string name;
  std::uint32_t vecSize;

  // These are managed automatically by Term
  std::unordered_set<Term *> sources;
  std::unordered_set<Term *> sinks;

  std::uint64_t nextTermIndex;
  std::vector<TermMapBase *> termMaps;

  // These members must currently be last, because their destruction triggers
  // associated Terms to be destructed, which still use the sources and sinks
  // structures above.
  // TODO: move away from shared ownership for Terms and have Program own them
  // uniquely. It is an error to hold onto a Term longer than a Program, but
  // the shared_ptr is misleading on this regard.
  std::unordered_map<std::string, Term::Ptr> outputs;
  std::unordered_map<std::string, Term::Ptr> inputs;

  friend class Term;
  friend class TermMapBase;
  friend std::unique_ptr<msg::Program> serialize(const Program &);
  friend std::unique_ptr<Program> deserialize(const msg::Program &);
};

std::unique_ptr<Program> deserialize(const msg::Program &);

} // namespace eva
