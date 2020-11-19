// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <utility>

namespace eva {

// The "Overloaded" trick to create convenient overloaded function objects for
// use in std::visit.
template <typename... Ts> struct Overloaded : Ts... {
  // Bring the various operator() overloads to this namespace
  using Ts::operator()...;
};

// Add a user-defined deduction guide for the class template
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

} // namespace eva
