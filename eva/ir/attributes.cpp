// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ir/attributes.h"
#include <stdexcept>

using namespace std;

namespace eva {

#define X(name, type) name::isValid(k, v) ||
bool isValidAttribute(AttributeKey k, const AttributeValue &v) {
  return EVA_ATTRIBUTES false;
}
#undef X

#define X(name, type)                                                          \
  case detail::name##Index:                                                    \
    return #name;
string getAttributeName(AttributeKey k) {
  switch (k) {
    EVA_ATTRIBUTES
  default:
    throw runtime_error("Unknown attribute key");
  }
}
#undef X

} // namespace eva
