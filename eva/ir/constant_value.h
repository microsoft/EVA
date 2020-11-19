// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/serialization/eva.pb.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace eva {

class ConstantValue {
public:
  ConstantValue(std::size_t size) : size(size) {}
  virtual ~ConstantValue() {}
  virtual const std::vector<double> &expand(std::vector<double> &scratch,
                                            std::size_t slots) const = 0;
  virtual void expandTo(std::vector<double> &result,
                        std::size_t slots) const = 0;
  virtual bool isZero() const = 0;
  virtual void serialize(msg::ConstantValue &msg) const = 0;

protected:
  std::size_t size;

  void validateSlots(std::size_t slots) const {
    if (slots < size) {
      throw std::runtime_error("Slots must be at least size of constant");
    }
    if (slots % size != 0) {
      throw std::runtime_error("Size must exactly divide slots");
    }
  }
};

class DenseConstantValue : public ConstantValue {
public:
  DenseConstantValue(std::size_t size, std::vector<double> values)
      : ConstantValue(size), values(values) {
    if (size % values.size() != 0) {
      throw std::runtime_error(
          "DenseConstantValue size must exactly divide size");
    }
  }

  const std::vector<double> &expand(std::vector<double> &scratch,
                                    std::size_t slots) const override {
    validateSlots(slots);
    if (values.size() == slots) {
      return values;
    } else {
      scratch.clear();
      for (int r = slots / values.size(); r > 0; --r) {
        scratch.insert(scratch.end(), values.begin(), values.end());
      }
      return scratch;
    }
  }

  void expandTo(std::vector<double> &result, std::size_t slots) const override {
    validateSlots(slots);
    result.clear();
    result.reserve(slots);
    for (int r = slots / values.size(); r > 0; --r) {
      result.insert(result.end(), values.begin(), values.end());
    }
  }

  bool isZero() const override {
    for (double value : values) {
      if (value != 0) return false;
    }
    return true;
  }

  void serialize(msg::ConstantValue &msg) const override {
    msg.set_size(size);
    auto valuesMsg = msg.mutable_values();
    valuesMsg->Reserve(values.size());
    for (const auto &value : values) {
      valuesMsg->Add(value);
    }
  }

private:
  std::vector<double> values;
};

class SparseConstantValue : public ConstantValue {
public:
  SparseConstantValue(std::size_t size,
                      std::vector<std::pair<std::uint32_t, double>> values)
      : ConstantValue(size), values(values) {}

  const std::vector<double> &expand(std::vector<double> &scratch,
                                    std::size_t slots) const override {
    validateSlots(slots);
    scratch.clear();
    scratch.resize(slots);
    for (auto &entry : values) {
      for (int i = 0; i < slots; i += values.size()) {
        scratch.at(entry.first + i) = entry.second;
      }
    }
    return scratch;
  }

  void expandTo(std::vector<double> &result, std::size_t slots) const override {
    validateSlots(slots);
    result.clear();
    result.resize(slots);
    for (auto &entry : values) {
      for (int i = 0; i < slots; i += values.size()) {
        result.at(entry.first + i) = entry.second;
      }
    }
  }

  bool isZero() const override {
    // TODO: this assumes no repeated indices
    for (auto entry : values) {
      if (entry.second != 0) return false;
    }
    return true;
  }

  void serialize(msg::ConstantValue &msg) const override {
    msg.set_size(size);
    for (const auto &pair : values) {
      msg.add_sparse_indices(pair.first);
      msg.add_values(pair.second);
    }
  }

private:
  std::vector<std::pair<std::uint32_t, double>> values;
};

std::unique_ptr<msg::ConstantValue> serialize(const ConstantValue &obj);
std::shared_ptr<ConstantValue> deserialize(const msg::ConstantValue &msg);

} // namespace eva
