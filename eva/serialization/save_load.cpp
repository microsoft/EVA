// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/serialization/save_load.h"

using namespace std;

namespace eva {

KnownType load(istream &in) {
  msg::KnownType msg;
  if (msg.ParseFromIstream(&in)) {
    return deserialize(msg);
  } else {
    throw runtime_error("Could not parse message");
  }
}

KnownType loadFromFile(const string &path) {
  ifstream in(path);
  if (in.fail()) {
    throw runtime_error("Could not open file");
  }
  return load(in);
}

KnownType loadFromString(const string &str) {
  msg::KnownType msg;
  if (msg.ParseFromString(str)) {
    return deserialize(msg);
  } else {
    throw runtime_error("Could not parse message");
  }
}

} // namespace eva
