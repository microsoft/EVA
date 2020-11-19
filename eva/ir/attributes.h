// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/attribute_list.h"
#include <cstdint>
#include <string>

namespace eva {

#define EVA_ATTRIBUTES                                                         \
  X(RescaleDivisorAttribute, std::uint32_t)                                    \
  X(RotationAttribute, std::int32_t)                                           \
  X(ConstantValueAttribute, std::shared_ptr<ConstantValue>)                    \
  X(TypeAttribute, Type)                                                       \
  X(RangeAttribute, std::uint32_t)                                             \
  X(EncodeAtScaleAttribute, std::uint32_t)                                     \
  X(EncodeAtLevelAttribute, std::uint32_t)

namespace detail {
enum AttributeIndex {
  RESERVE_EMPTY_ATTRIBUTE_KEY = 0,
#define X(name, type) name##Index,
  EVA_ATTRIBUTES
#undef X
};
} // namespace detail

#define X(name, type) using name = Attribute<detail::name##Index, type>;
EVA_ATTRIBUTES
#undef X

bool isValidAttribute(AttributeKey k, const AttributeValue &v);

std::string getAttributeName(AttributeKey k);

} // namespace eva
