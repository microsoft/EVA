// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/constant_value.h"
#include "eva/ir/types.h"
#include "eva/serialization/eva.pb.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <variant>

namespace eva {

using AttributeValue = std::variant<std::monostate, std::uint32_t, std::int32_t,
                                    Type, std::shared_ptr<ConstantValue>>;

template <class T, class TypeList> struct IsInVariant;
template <class T, class... Ts>
struct IsInVariant<T, std::variant<Ts...>>
    : std::bool_constant<(... || std::is_same<T, Ts>{})> {};

using AttributeKey = std::uint8_t;

template <std::uint64_t Key, class T> struct Attribute {
  static_assert(IsInVariant<T, AttributeValue>::value,
                "Attribute type not in AttributeValue");
  static_assert(Key > 0, "Keys must be strictly positive");
  static_assert(Key <= std::numeric_limits<AttributeKey>::max(),
                "Key larger than current AttributeKey type");

  using Value = T;
  static constexpr AttributeKey key = Key;

  static bool isValid(AttributeKey k, const AttributeValue &v) {
    return k == Key && std::holds_alternative<T>(v);
  }
};

class AttributeList {
public:
  AttributeList() : key(0), tail(nullptr) {}

  // This function is defined in eva/serialization/eva_serialization.cpp
  void loadAttribute(const msg::Attribute &msg);

  // This function is defined in eva/serialization/eva_serialization.cpp
  void serializeAttributes(std::function<msg::Attribute *()> addMsg) const;

  template <class TAttr> bool has() const { return has(TAttr::key); }

  template <class TAttr> const typename TAttr::Value &get() const {
    return std::get<typename TAttr::Value>(get(TAttr::key));
  }

  template <class TAttr> void set(typename TAttr::Value value) {
    set(TAttr::key, std::move(value));
  }

  void assignAttributesFrom(const AttributeList &other);

private:
  AttributeKey key;
  AttributeValue value;
  std::unique_ptr<AttributeList> tail;

  AttributeList(AttributeKey k, AttributeValue v)
      : key(k), value(std::move(v)) {}

  bool has(AttributeKey k) const;
  const AttributeValue &get(AttributeKey k) const;
  void set(AttributeKey k, AttributeValue v);
};

} // namespace eva
