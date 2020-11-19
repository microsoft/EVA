// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "eva/eva.h"
#include "eva/common/program_traversal.h"
#include "eva/common/reference_executor.h"
#include "eva/common/valuation.h"

namespace eva {

Valuation evaluate(Program &program, const Valuation &inputs) {
  Valuation outputs;
  ProgramTraversal programTraverse(program);
  ReferenceExecutor ge(program);

  ge.setInputs(inputs);
  programTraverse.forwardPass(ge);
  ge.getOutputs(outputs);

  return outputs;
}

} // namespace eva
