// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/ckks_compiler.h"
#include "eva/ir/program.h"
#include "eva/seal/seal.h"
#include "eva/serialization/save_load.h"
#include "eva/version.h"

namespace eva {

Valuation evaluate(Program &program, const Valuation &inputs);

}
