// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/ckks/ckks_parameters.h"
#include "eva/ckks/ckks_signature.h"
#include "eva/serialization/ckks.pb.h"
#include <memory>
#include <utility>

using namespace std;

namespace eva {

unique_ptr<msg::CKKSParameters> serialize(const CKKSParameters &obj) {
  // Create a new protobuf message
  auto msg = make_unique<msg::CKKSParameters>();

  // Save the prime bit counts
  auto primeBitsMsg = msg->mutable_prime_bits();
  primeBitsMsg->Reserve(obj.primeBits.size());
  for (const auto &bits : obj.primeBits) {
    primeBitsMsg->Add(bits);
  }

  // Save the rotations that are needed
  auto rotationsMsg = msg->mutable_rotations();
  rotationsMsg->Reserve(obj.rotations.size());
  for (const auto &rotation : obj.rotations) {
    rotationsMsg->Add(rotation);
  }

  // Save the polynomial modulus degree
  msg->set_poly_modulus_degree(obj.polyModulusDegree);

  return msg;
}

unique_ptr<CKKSParameters> deserialize(const msg::CKKSParameters &msg) {
  // Create a new CKKSParameters object
  auto obj = make_unique<CKKSParameters>();

  // Load the values from the protobuf message
  obj->primeBits = {msg.prime_bits().begin(), msg.prime_bits().end()};
  obj->rotations = {msg.rotations().begin(), msg.rotations().end()};
  obj->polyModulusDegree = msg.poly_modulus_degree();

  return obj;
}

unique_ptr<msg::CKKSSignature> serialize(const CKKSSignature &obj) {
  // Create a new protobuf message
  auto msg = make_unique<msg::CKKSSignature>();

  // Save the vector size
  msg->set_vec_size(obj.vecSize);

  // Save the input map
  auto &inputsMap = *msg->mutable_inputs();
  for (auto &[key, info] : obj.inputs) {
    auto &infoMsg = inputsMap[key];
    infoMsg.set_input_type(static_cast<int32_t>(info.inputType));
    infoMsg.set_scale(info.scale);
    infoMsg.set_level(info.level);
  }

  return msg;
}

unique_ptr<CKKSSignature> deserialize(const msg::CKKSSignature &msg) {
  // Create a new map of CKKSEncodingInfo objects and load the data
  unordered_map<string, CKKSEncodingInfo> inputs;
  for (auto &[key, infoMsg] : msg.inputs()) {
    inputs.emplace(key,
                   CKKSEncodingInfo(static_cast<Type>(infoMsg.input_type()),
                                    infoMsg.scale(), infoMsg.level()));
  }

  // Return a new CKKSSignature object
  return make_unique<CKKSSignature>(msg.vec_size(), move(inputs));
}

} // namespace eva
