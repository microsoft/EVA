// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/ckks_parameters.h"
#include "eva/ckks/ckks_signature.h"
#include "eva/common/valuation.h"
#include "eva/ir/program.h"
#include "eva/serialization/seal.pb.h"
#include <cassert>
#include <memory>
#include <seal/seal.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>

namespace eva {

using SchemeValue = std::variant<seal::Ciphertext, seal::Plaintext,
                                 std::shared_ptr<ConstantValue>>;

class SEALValuation {
public:
  SEALValuation(const seal::EncryptionParameters &params) : params(params) {}
  SEALValuation(const seal::SEALContext &context)
      : params(context.key_context_data()->parms()) {}

  auto &operator[](const std::string &name) { return values[name]; }
  auto begin() { return values.begin(); }
  auto begin() const { return values.begin(); }
  auto end() { return values.end(); }
  auto end() const { return values.end(); }

private:
  seal::EncryptionParameters params;
  std::unordered_map<std::string, SchemeValue> values;

  friend std::unique_ptr<msg::SEALValuation> serialize(const SEALValuation &);
};

std::unique_ptr<SEALValuation> deserialize(const msg::SEALValuation &);

class SEALPublic {
public:
  SEALPublic(seal::SEALContext ctx, seal::PublicKey pk, seal::GaloisKeys gk,
             seal::RelinKeys rk)
      : context(ctx), publicKey(pk), galoisKeys(gk), relinKeys(rk),
        encoder(ctx), encryptor(ctx, publicKey), evaluator(ctx) {}

  SEALValuation encrypt(const Valuation &inputs,
                        const CKKSSignature &signature);

  SEALValuation execute(Program &program, const SEALValuation &inputs);

private:
  seal::SEALContext context;

  seal::PublicKey publicKey;
  seal::GaloisKeys galoisKeys;
  seal::RelinKeys relinKeys;

  seal::CKKSEncoder encoder;
  seal::Encryptor encryptor;
  seal::Evaluator evaluator;

  friend std::unique_ptr<msg::SEALPublic> serialize(const SEALPublic &);
};

std::unique_ptr<SEALPublic> deserialize(const msg::SEALPublic &);

class SEALSecret {
public:
  SEALSecret(seal::SEALContext ctx, seal::SecretKey sk)
      : context(ctx), secretKey(sk), encoder(ctx), decryptor(ctx, secretKey) {}

  Valuation decrypt(const SEALValuation &encOutputs,
                    const CKKSSignature &signature);

private:
  seal::SEALContext context;

  seal::SecretKey secretKey;

  seal::CKKSEncoder encoder;
  seal::Decryptor decryptor;

  friend std::unique_ptr<msg::SEALSecret> serialize(const SEALSecret &);
};

std::unique_ptr<SEALSecret> deserialize(const msg::SEALSecret &);

seal::SEALContext getSEALContext(const seal::EncryptionParameters &params);

std::tuple<std::unique_ptr<SEALPublic>, std::unique_ptr<SEALSecret>>
generateKeys(const CKKSParameters &abstractParams);

} // namespace eva
