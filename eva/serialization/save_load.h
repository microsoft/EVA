// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/serialization/known_type.h"
#include "eva/version.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace eva {

KnownType load(std::istream &in);
KnownType loadFromFile(const std::string &path);
KnownType loadFromString(const std::string &str);

template <class T> T load(std::istream &in) { return std::get<T>(load(in)); }

template <class T> T loadFromFile(const std::string &path) {
  return std::get<T>(loadFromFile(path));
}

template <class T> T loadFromString(const std::string &str) {
  return std::get<T>(loadFromString(str));
}

namespace detail {
template <class T> void serializeKnownType(const T &obj, msg::KnownType &msg) {
  auto inner = serialize(obj);
  msg.set_creator("EVA " + version());
  msg.mutable_contents()->PackFrom(*inner);
}
} // namespace detail

template <class T> void save(const T &obj, std::ostream &out) {
  msg::KnownType msg;
  detail::serializeKnownType(obj, msg);
  if (!msg.SerializeToOstream(&out)) {
    throw std::runtime_error("Could not serialize message");
  }
}

template <class T> void saveToFile(const T &obj, const std::string &path) {
  std::ofstream out(path);
  if (out.fail()) {
    throw std::runtime_error("Could not open file");
  }
  save(obj, out);
}

template <class T> std::string saveToString(const T &obj) {
  msg::KnownType msg;
  detail::serializeKnownType(obj, msg);
  std::string str;
  if (msg.SerializeToString(&str)) {
    return str;
  } else {
    throw std::runtime_error("Could not serialize message");
  }
}

} // namespace eva
