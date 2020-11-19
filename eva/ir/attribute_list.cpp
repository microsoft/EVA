// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ir/attribute_list.h"
#include "eva/ir/attributes.h"
#include "eva/util/overloaded.h"
#include <stdexcept>

using namespace std;

namespace eva {

bool AttributeList::has(AttributeKey k) const {
  const AttributeList *curr = this;
  while (true) {
    if (curr->key < k) {
      if (curr->tail) {
        curr = curr->tail.get();
      } else {
        return false;
      }
    } else {
      return curr->key == k;
    }
  }
}

const AttributeValue &AttributeList::get(AttributeKey k) const {
  const AttributeList *curr = this;
  while (true) {
    if (curr->key == k) {
      return curr->value;
    } else if (curr->key < k && curr->tail) {
      curr = curr->tail.get();
    } else {
      throw out_of_range("Attribute not in list: " + getAttributeName(k));
    }
  }
}

void AttributeList::set(AttributeKey k, AttributeValue v) {
  if (this->key == 0) {
    this->key = k;
    this->value = move(v);
  } else {
    AttributeList *curr = this;
    AttributeList *prev = nullptr;
    while (true) {
      if (curr->key < k) {
        if (curr->tail) {
          prev = curr;
          curr = curr->tail.get();
        } else { // Insert at end
          // AttributeList constructor is private
          curr->tail = unique_ptr<AttributeList>{new AttributeList(k, move(v))};
          return;
        }
      } else if (curr->key > k) {
        if (prev) { // Insert between
          // AttributeList constructor is private
          auto newList =
              unique_ptr<AttributeList>{new AttributeList(k, move(v))};
          newList->tail = move(prev->tail);
          prev->tail = move(newList);
        } else { // Insert at beginning
          // AttributeList constructor is private
          curr->tail =
              unique_ptr<AttributeList>{new AttributeList(move(*curr))};
          curr->key = k;
          curr->value = move(v);
        }
        return;
      } else {
        assert(curr->key == k);
        curr->value = move(v);
        return;
      }
    }
  }
}

void AttributeList::assignAttributesFrom(const AttributeList &other) {
  if (this->key != 0) {
    this->key = 0;
    this->value = std::monostate();
    this->tail = nullptr;
  }
  if (other.key == 0) {
    return;
  }
  AttributeList *lhs = this;
  const AttributeList *rhs = &other;
  while (true) {
    lhs->key = rhs->key;
    lhs->value = rhs->value;
    if (rhs->tail) {
      rhs = rhs->tail.get();
      lhs->tail = std::make_unique<AttributeList>();
      lhs = lhs->tail.get();
    } else {
      return;
    }
  }
}

} // namespace eva
