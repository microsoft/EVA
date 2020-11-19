// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/program.h"
#include "eva/ir/term.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <variant>
#include <vector>

namespace eva {

class TermMapBase {
public:
  TermMapBase(Program &p) : program(&p) { program->registerTermMap(this); }
  ~TermMapBase() { program->unregisterTermMap(this); }
  TermMapBase(const TermMapBase &other) : program(other.program) {
    program->registerTermMap(this);
  }
  TermMapBase &operator=(const TermMapBase &other) = default;

  friend class Program;

protected:
  void init() { program->initTermMap(*this); }

  std::uint64_t getIndex(const Term &term) const { return term.index; }

private:
  virtual void resize(std::size_t size) = 0;

  Program *program;
};

template <class TValue> class TermMap : TermMapBase {
public:
  TermMap(Program &p) : TermMapBase(p) { init(); }

  TValue &operator[](const Term &term) { return values.at(getIndex(term)); }

  const TValue &operator[](const Term &term) const {
    return values.at(getIndex(term));
  }

  TValue &operator[](const Term::Ptr &term) { return this->operator[](*term); }

  const TValue &operator[](const Term::Ptr &term) const {
    return this->operator[](*term);
  }

  void clear() { values.assign(values.size(), {}); }

private:
  void resize(std::size_t size) override { values.resize(size); }

  std::deque<TValue> values;
};

template <> class TermMap<bool> : TermMapBase {
public:
  TermMap(Program &p) : TermMapBase(p) { init(); }

  std::vector<bool>::reference operator[](const Term &term) {
    return values.at(getIndex(term));
  }

  bool operator[](const Term &term) const { return values.at(getIndex(term)); }

  std::vector<bool>::reference operator[](const Term::Ptr &term) {
    return this->operator[](*term);
  }

  bool operator[](const Term::Ptr &term) const {
    return this->operator[](*term);
  }

  void clear() { values.assign(values.size(), false); }

private:
  void resize(std::size_t size) override { values.resize(size); }

  std::vector<bool> values;
};

template <class TOptionalValue> class TermMapOptional : TermMapBase {
public:
  TermMapOptional(Program &p) : TermMapBase(p) { init(); }

  TOptionalValue &operator[](const Term &term) {
    auto &value = values.at(getIndex(term));
    if (!value.has_value()) {
      value.emplace();
    }
    return *value;
  }

  TOptionalValue &operator[](const Term::Ptr &term) {
    return this->operator[](*term);
  }

  TOptionalValue &at(const Term &term) {
    return values.at(getIndex(term)).value();
  }

  TOptionalValue &at(const Term::Ptr &term) { return this->at(*term); }

  bool has(const Term &term) const {
    return values.at(getIndex(term)).has_value();
  }

  bool has(const Term::Ptr &term) const { return has(*term); }

  void clear() { values.assign(values.size(), std::nullopt); }

private:
  void resize(std::size_t size) override { values.resize(size); }

  std::deque<std::optional<TOptionalValue>> values;
};

} // namespace eva
