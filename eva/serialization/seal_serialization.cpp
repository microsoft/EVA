// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/seal/seal.h"
#include "eva/util/overloaded.h"
#include <memory>
#include <string>
#include <variant>

using namespace std;

namespace eva {

using SEALObject = msg::SEALObject;

template <class T> auto getSEALTypeTag();

template <> auto getSEALTypeTag<seal::Ciphertext>() {
  return SEALObject::CIPHERTEXT;
}

template <> auto getSEALTypeTag<seal::Plaintext>() {
  return SEALObject::PLAINTEXT;
}

template <> auto getSEALTypeTag<seal::SecretKey>() {
  return SEALObject::SECRET_KEY;
}

template <> auto getSEALTypeTag<seal::PublicKey>() {
  return SEALObject::PUBLIC_KEY;
}

template <> auto getSEALTypeTag<seal::GaloisKeys>() {
  return SEALObject::GALOIS_KEYS;
}

template <> auto getSEALTypeTag<seal::RelinKeys>() {
  return SEALObject::RELIN_KEYS;
}

template <> auto getSEALTypeTag<seal::EncryptionParameters>() {
  return SEALObject::ENCRYPTION_PARAMETERS;
}

template <class T> void serializeSEALType(const T &obj, SEALObject *msg) {
  // Get an upper bound for the size from SEAL; use default compression mode
  auto maxSize = obj.save_size(seal::Serialization::compr_mode_default);

  // Set up a buffer (std::string)
  // We allocate the string into a std::unique_ptr and eventually pass ownership
  // to the Protobuf message below
  auto data = make_unique<string>();
  data->resize(maxSize);

  // Note, since C++11 std::string is guaranteed to be contiguous
  auto actualSize =
      obj.save(reinterpret_cast<seal::seal_byte *>(&data->operator[](0)),
               maxSize, seal::Serialization::compr_mode_default);
  data->resize(actualSize);

  // Change ownership of the data string to msg
  msg->set_allocated_data(data.release());

  // Set the type tag to indicate the SEAL object type
  msg->set_seal_type(getSEALTypeTag<T>());
}

template <class T> void deserializeSEALType(T &obj, const SEALObject &msg) {
  // Unknown type; throw
  if (msg.seal_type() == SEALObject::UNKNOWN) {
    throw runtime_error("SEAL message type set to UNKNOWN");
  }

  // Type of obj is incompatible with the type indicated in msg
  if (msg.seal_type() != getSEALTypeTag<T>()) {
    throw runtime_error("SEAL message type mismatch");
  }

  // Load the SEAL object
  obj.load(reinterpret_cast<const seal::seal_byte *>(msg.data().c_str()),
           msg.data().size());
}

template <class T>
void deserializeSEALTypeWithContext(const seal::SEALContext &context, T &obj,
                                    const SEALObject &msg) {
  // Most SEAL objects require the SEALContext for safe loading
  // Unknown type; throw
  if (msg.seal_type() == SEALObject::UNKNOWN) {
    throw runtime_error("SEAL message type set to UNKNOWN");
  }

  // Type of obj is incompatible with the type indicated in msg
  if (msg.seal_type() != getSEALTypeTag<T>()) {
    throw runtime_error("SEAL message type mismatch");
  }

  // Load the SEAL object and check its validity against given context
  obj.load(context,
           reinterpret_cast<const seal::seal_byte *>(msg.data().c_str()),
           msg.data().size());
}

unique_ptr<SEALValuation> deserialize(const msg::SEALValuation &msg) {
  // Deserialize a SEAL valuation: either plaintexts or ciphertexts
  // First need to load the encryption parameters and obtain the context
  seal::EncryptionParameters encParams;
  deserializeSEALType(encParams, msg.encryption_parameters());
  auto context = getSEALContext(encParams);

  // Create the destination valuation and load the correct type
  auto obj = make_unique<SEALValuation>(encParams);
  for (const auto &entry : msg.values()) {
    auto &value = obj->operator[](entry.first);

    // Load the correct kind of object based on value
    switch (entry.second.seal_type()) {
    case SEALObject::CIPHERTEXT: {
      value = seal::Ciphertext();
      deserializeSEALTypeWithContext(context, get<seal::Ciphertext>(value),
                                     entry.second);
      break;
    }
    case SEALObject::PLAINTEXT: {
      value = seal::Plaintext();
      deserializeSEALTypeWithContext(context, get<seal::Plaintext>(value),
                                     entry.second);
      break;
    }
    default:
      throw runtime_error("Not a ciphertext or plaintext");
    }
  }

  // Deserialize the raw part of the valuation
  for (const auto &entry : msg.raw_values()) {
    obj->operator[](entry.first) = deserialize(entry.second);
  }

  return obj;
}

unique_ptr<msg::SEALValuation> serialize(const SEALValuation &obj) {
  // Create the Protobuf message and save the encryption parameters
  auto msg = make_unique<msg::SEALValuation>();
  serializeSEALType(obj.params, msg->mutable_encryption_parameters());
  // Serialize a SEAL valuation: either plaintexts or ciphertexts
  auto &valuesMsg = *msg->mutable_values();
  auto &rawValuesMsg = *msg->mutable_raw_values();
  for (const auto &entry : obj) {
    // Visit entry.second with an overloaded lambda function; we need to specify
    // handling for both possible data types (plaintexts and ciphertexts)
    visit(Overloaded{[&](const seal::Ciphertext &cipher) {
                       serializeSEALType(cipher, &valuesMsg[entry.first]);
                     },
                     [&](const seal::Plaintext &plain) {
                       serializeSEALType(plain, &valuesMsg[entry.first]);
                     },
                     [&](const std::shared_ptr<ConstantValue> raw) {
                       raw->serialize(rawValuesMsg[entry.first]);
                     }},
          entry.second);
  }

  return msg;
}

unique_ptr<msg::SEALPublic> serialize(const SEALPublic &obj) {
  // Serialize a SEALPublic object
  auto msg = make_unique<msg::SEALPublic>();

  // Save the encryption parameters
  serializeSEALType(obj.context.key_context_data()->parms(),
                    msg->mutable_encryption_parameters());

  // Save the different public keys
  serializeSEALType(obj.publicKey, msg->mutable_public_key());
  serializeSEALType(obj.galoisKeys, msg->mutable_galois_keys());
  serializeSEALType(obj.relinKeys, msg->mutable_relin_keys());

  return msg;
}

unique_ptr<SEALPublic> deserialize(const msg::SEALPublic &msg) {
  // Deserialize a SEALPublic object
  // Load the encryption parameters and acquire a SEALContext; this is needed
  // for safe loading of the other objects
  seal::EncryptionParameters encParams;
  deserializeSEALType(encParams, msg.encryption_parameters());
  auto context = getSEALContext(encParams);

  // Load the different public keys
  seal::PublicKey pk;
  deserializeSEALTypeWithContext(context, pk, msg.public_key());
  seal::GaloisKeys gk;
  deserializeSEALTypeWithContext(context, gk, msg.galois_keys());
  seal::RelinKeys rk;
  deserializeSEALTypeWithContext(context, rk, msg.relin_keys());

  return make_unique<SEALPublic>(context, pk, gk, rk);
}

unique_ptr<msg::SEALSecret> serialize(const SEALSecret &obj) {
  // Serialize a SEALSecret object
  auto msg = make_unique<msg::SEALSecret>();

  // Save the encryption parameters
  serializeSEALType(obj.context.key_context_data()->parms(),
                    msg->mutable_encryption_parameters());

  // Save the secret key
  serializeSEALType(obj.secretKey, msg->mutable_secret_key());
  return msg;
}

unique_ptr<SEALSecret> deserialize(const msg::SEALSecret &msg) {
  // Deserialize a SEALSecret object
  // Load the encryption parameters and acquire a SEALContext; this is needed
  // for safe loading of the other objects
  seal::EncryptionParameters encParams;
  deserializeSEALType(encParams, msg.encryption_parameters());
  auto context = getSEALContext(encParams);

  // Load the secret key
  seal::SecretKey sk;
  deserializeSEALTypeWithContext(context, sk, msg.secret_key());

  return make_unique<SEALSecret>(context, sk);
}

} // namespace eva
