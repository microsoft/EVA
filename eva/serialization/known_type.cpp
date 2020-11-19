// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/serialization/known_type.h"

using namespace std;

namespace eva {

namespace {

inline void dispatchKnownTypeDeserialize(KnownType &obj,
                                         const msg::KnownType &msg) {
  // Try loading msg until the correct type is found
  EVA_KNOWN_TYPE_TRY_DESERIALIZE(msg::Program);
  EVA_KNOWN_TYPE_TRY_DESERIALIZE(msg::CKKSParameters);
  EVA_KNOWN_TYPE_TRY_DESERIALIZE(msg::CKKSSignature);
  EVA_KNOWN_TYPE_TRY_DESERIALIZE(msg::SEALValuation);
  EVA_KNOWN_TYPE_TRY_DESERIALIZE(msg::SEALPublic);
  EVA_KNOWN_TYPE_TRY_DESERIALIZE(msg::SEALSecret);

  // This is not a known type
  throw runtime_error("Unknown inner message type " +
                      msg.contents().type_url());
}

} // namespace

KnownType deserialize(const msg::KnownType &msg) {
  KnownType obj;
  dispatchKnownTypeDeserialize(obj, msg);
  return obj;
}

} // namespace eva
