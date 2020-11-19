// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ir/attribute_list.h"
#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include "eva/serialization/eva_format_version.h"
#include "eva/util/overloaded.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

namespace eva {

// Definition for member function AttributeList::loadAttribute
// The function definition is here so that all serialization code is together
// and not spread out throughout the project
void AttributeList::loadAttribute(const msg::Attribute &msg) {
  // Load the attribute key; this encodes the type of the attribute
  AttributeKey key = static_cast<AttributeKey>(msg.key());

  // A variant that holds the possible value types for attributes
  AttributeValue value;

  switch (msg.value_case()) {
  case msg::Attribute::kUint32:
    // The attribute holds a uint32; load it
    value.emplace<uint32_t>(msg.uint32());
    break;
  case msg::Attribute::kInt32:
    // The attribute holds an int32; load it
    value.emplace<int32_t>(msg.int32());
    break;
  case msg::Attribute::kType:
    // The attribute holds a Type (see eva/ir/types.h); load it
    value.emplace<Type>(static_cast<Type>(msg.type()));
    break;
  case msg::Attribute::kConstantValue:
    // The attribute holds a constant value; load it
    value.emplace<shared_ptr<ConstantValue>>(deserialize(msg.constant_value()));
    break;
  case msg::Attribute::VALUE_NOT_SET:
    // No value is set; set the attribute to std::monostate
    value.emplace<monostate>();
    break;
  default:
    // An unexpected value
    throw runtime_error("Unknown attribute type");
  }

  // Check that the attribute value is valid
  if (!isValidAttribute(key, value)) {
    throw runtime_error("Invalid attribute encountered");
  }

  // Add the attribute to this AttributeList
  set(key, move(value));
}

// Definition for member function AttributeList::serializeAttributes
// The function definition is here so that all serialization code is together
// and not spread out throughout the project
void AttributeList::serializeAttributes(
    function<msg::Attribute *()> addMsg) const {
  // Nothing to do if key is zero (empty attribute; see eva/ir/attributes.h)
  if (key == 0) {
    return;
  }

  // Go over each attribute in this list
  const AttributeList *curr = this;
  do {
    // Get a pointer to a new attribute
    msg::Attribute *msg = addMsg();

    // Set the key and value
    msg->set_key(curr->key);
    visit(Overloaded{[&](const monostate &value) {
                       // This is an empty attribute
                     },
                     [&](const uint32_t &value) { msg->set_uint32(value); },
                     [&](const int32_t &value) { msg->set_int32(value); },
                     [&](const Type &value) {
                       msg->set_type(static_cast<uint32_t>(value));
                     },
                     [&](const shared_ptr<ConstantValue> &value) {
                       auto valueMsg = serialize(*value);
                       msg->set_allocated_constant_value(valueMsg.release());
                     }},
          curr->value);

    // Move on to the next attribute
    curr = curr->tail.get();
  } while (curr);
}

unique_ptr<msg::ConstantValue> serialize(const ConstantValue &obj) {
  // Save a constant value; the implementations are in eva/ir/constant_value.h
  auto msg = std::make_unique<msg::ConstantValue>();
  obj.serialize(*msg);
  return msg;
}

shared_ptr<ConstantValue> deserialize(const msg::ConstantValue &msg) {
  if (msg.size() == 0) {
    throw runtime_error("Constant must have non-zero size");
  }

  size_t size = msg.size();
  if (msg.values_size() == 0) {
    // Zero size; return a sparse zero constant
    return make_shared<SparseConstantValue>(size,
                                            vector<pair<uint32_t, double>>{});
  } else if (msg.sparse_indices_size() == 0) {
    // No sparse indices so this is a dense constant
    vector<double> values(msg.values().begin(), msg.values().end());
    return make_shared<DenseConstantValue>(size, move(values));
  } else {
    // Must be a sparse constant; check that the data is consistent
    if (msg.sparse_indices_size() != msg.values_size()) {
      throw runtime_error("Values and sparse indices count mismatch");
    }

    // Load the sparse representation
    vector<pair<uint32_t, double>> values;
    auto indexIter = msg.sparse_indices().begin();
    auto valueIter = msg.values().begin();
    while (indexIter != msg.sparse_indices().end()) {
      values.emplace_back(*indexIter, *valueIter);
      ++indexIter;
      ++valueIter;
    }

    return make_shared<SparseConstantValue>(size, move(values));
  }
}

unique_ptr<msg::Program> serialize(const Program &obj) {
  // Create a new program message for serialization
  auto msg = make_unique<msg::Program>();

  // Save the IR version and vector size
  msg->set_ir_version(EVA_FORMAT_VERSION);
  msg->set_vec_size(obj.vecSize);

  // Save all terms in topologically sorted order; this is convenient so we can
  // easily load it back and set up operand pointers immediately after loading
  // a term. To each term we assign a topological index (operands of each term
  // have indices less than the index of the current term). The edges of the
  // program graph are saved by providing the operand term indices for each
  // term.

  // Table of topological indices assigned to the terms
  unordered_map<Term *, uint64_t> indices;

  // Index to be assigned next
  uint64_t nextIndex = 0;

  // Work stack of terms that need to be processed
  // The bool ("visit") signals whether the operands of the term have already
  // been processed. If this is false, we are ready to give the term an index.
  stack<pair<bool, Term *>> work;

  // Add each sink to the work stack with visit flag set to true
  for (const auto &sink : obj.getSinks()) {
    work.emplace(true, sink.get());
  }

  // Operate on the stack until empty
  while (!work.empty()) {
    // Pop from the stack
    bool visit = work.top().first;
    auto term = work.top().second;
    work.pop();

    // If this term has already been given an index, there is nothing to do;
    // all of its operands are guaranteed to have been assigned indices
    if (indices.count(term)) {
      continue;
    }

    // This term does not yet appear in the index map
    // Do we need to process its operands or are we ready to assign an index?
    if (visit) {
      // Add the term back with to-do/visit flag set to false; next time we
      // process this term we will give it an index
      work.emplace(false, term);

      // Add the operands to work stack with visit flag set to true
      for (const auto &operand : term->getOperands()) {
        work.emplace(true, operand.get());
      }
    } else {
      // The operands of this term have already been processed; ready to assign
      // an index

      // Read the current index value and increment the counter for next
      auto index = nextIndex;
      nextIndex += 1;

      // Add this term to the indices map
      indices[term] = index;

      // Add a new term to the message; set the opcode and add operands
      auto termMsg = msg->add_terms();
      termMsg->set_op(static_cast<uint32_t>(term->op));
      for (const auto &operand : term->getOperands()) {
        // Add operands to the current term by saving the indices
        termMsg->add_operands(indices.at(operand.get()));
      }

      // Save the attributes for this term
      term->serializeAttributes([&]() { return termMsg->add_attributes(); });
    }
  }

  // Save the input term indices and labels
  for (const auto &entry : obj.inputs) {
    auto termNameMsg = msg->add_inputs();
    termNameMsg->set_name(entry.first);
    termNameMsg->set_term(indices.at(entry.second.get()));
  }

  // Save the output term indices and labels
  for (const auto &entry : obj.outputs) {
    auto termNameMsg = msg->add_outputs();
    termNameMsg->set_name(entry.first);
    termNameMsg->set_term(indices.at(entry.second.get()));
  }

  return msg;
}

unique_ptr<Program> deserialize(const msg::Program &msg) {
  // Ensure serialization version is compatible
  if (msg.ir_version() != EVA_FORMAT_VERSION) {
    throw runtime_error("Serialization format version mismatch");
  }

  // Create a new program with the loaded name and vector size
  auto obj = make_unique<Program>(msg.name(), msg.vec_size());

  // Create a vector of term pointers
  vector<Term::Ptr> terms;

  for (auto &term : msg.terms()) {
    // The terms were saved in topologically sorted order, so the operands of
    // the current term were already loaded and their pointers are in the terms
    // vector. Moreover, the serialized indices match the saving/loading order,
    // i.e., the index in the terms vector.

    // Check opcode validity
    auto op = static_cast<Op>(term.op());
    if (!isValidOp(op)) {
      throw runtime_error("Invalid op encountered");
    }

    // Create a new term and set its operands (already loaded)
    terms.emplace_back(obj->makeTerm(op));
    for (auto &operandIdx : term.operands()) {
      terms.back()->addOperand(terms.at(operandIdx));
    }

    // Load attributes for this term
    for (auto &attribute : term.attributes()) {
      terms.back()->loadAttribute(attribute);
    }
  }

  // Set the inputs
  for (auto &in : msg.inputs()) {
    obj->inputs.emplace(in.name(), terms.at(in.term()));
  }

  // Set the outputs
  for (auto &out : msg.outputs()) {
    obj->outputs.emplace(out.name(), terms.at(out.term()));
  }

  return obj;
}

} // namespace eva
