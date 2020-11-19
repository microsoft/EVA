// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/util/galois.h"

namespace eva {

GaloisGuard::GaloisGuard() {
  // Galois doesn't exit quietly, so lets just leak it instead.
  // It was also crashing on exit when this decision was made.
  static galois::SharedMemSys *galois = new galois::SharedMemSys();
}

} // namespace eva
