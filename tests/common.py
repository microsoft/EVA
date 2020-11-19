# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

import unittest
from random import uniform
from eva import evaluate
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
from eva.metric import valuation_mse

class EvaTestCase(unittest.TestCase):
    def assert_compiles_and_matches_reference(self, prog, inputs = None, config={}):
        if inputs == None:
            inputs = { name: [uniform(-2,2) for _ in range(prog.vec_size)]
                for name in prog.inputs }
        config['warn_vec_size'] = 'false'

        reference = evaluate(prog, inputs)

        compiler = CKKSCompiler(config = config)
        compiled_prog, params, signature = compiler.compile(prog)

        reference_compiled = evaluate(compiled_prog, inputs)
        ref_mse = valuation_mse(reference, reference_compiled)
        self.assertTrue(ref_mse < 0.0000000001,
            f"Mean squared error was {ref_mse}")

        public_ctx, secret_ctx = generate_keys(params)
        encInputs = public_ctx.encrypt(inputs, signature)
        encOutputs = public_ctx.execute(compiled_prog, encInputs)
        outputs = secret_ctx.decrypt(encOutputs, signature)

        he_mse = valuation_mse(outputs, reference)
        self.assertTrue(he_mse < 0.01, f"Mean squared error was {he_mse}")

        return (compiled_prog, params, signature)