# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

import unittest
import tempfile
import os
from common import *
from eva import EvaProgram, Input, Output, save, load

class Features(EvaTestCase):
    def test_bin_ops(self):
        """ Test all binary ops """

        for binOp in [lambda a, b: a + b, lambda a, b: a - b, lambda a, b: a * b]:
            for enc1 in [False, True]:
                for enc2 in [False, True]:
                    prog = EvaProgram('BinOp', vec_size = 64)
                    with prog:
                        a = Input('a', enc1)
                        b = Input('b', enc2)
                        Output('y', binOp(a,b))

                    prog.set_output_ranges(20)
                    prog.set_input_scales(30)

                    self.assert_compiles_and_matches_reference(prog,
                        config={'warn_vec_size':'false'})
    
    def test_unary_ops(self):
        """ Test all unary ops """

        for unOp in [lambda x: x, lambda x: -x, lambda x: x**3, lambda x: 42]:
            for enc in [False, True]:
                prog = EvaProgram('UnOp', vec_size = 64)
                with prog:
                    x = Input('x', enc)
                    Output('y', unOp(x))

                prog.set_output_ranges(20)
                prog.set_input_scales(30)

                self.assert_compiles_and_matches_reference(prog,
                    config={'warn_vec_size':'false'})

    def test_rotations(self):
        """ Test all rotations """

        for rotOp in [lambda x, r: x << r, lambda x, r: x >> r]:
            for enc in [False, True]:
                for rot in range(-2,2):
                    prog = EvaProgram('RotOp', vec_size = 8)
                    with prog:
                        x = Input('x')
                        Output('y', rotOp(x,rot))

                    prog.set_output_ranges(20)
                    prog.set_input_scales(30)

                    self.assert_compiles_and_matches_reference(prog,
                        config={'warn_vec_size':'false'})

    def test_unencrypted_computation(self):
        """ Test computation on unencrypted values """

        for enc1 in [False, True]:
            for enc2 in [False, True]:
                prog = EvaProgram('UnencryptedInputs', vec_size=128)
                with prog:
                    x1 = Input('x1', enc1)
                    x2 = Input('x2', enc2)
                    Output('y', pow(x2,3) + x1*x2)

                prog.set_output_ranges(20)
                prog.set_input_scales(30)

                self.assert_compiles_and_matches_reference(prog,
                    config={'warn_vec_size':'false'})

    def test_security_levels(self):
        """ Check that all supported security levels work """

        security_levels = ['128','192','256']
        quantum_safety = ['false','true']

        for s in security_levels:
            for q in quantum_safety:
                prog = EvaProgram('SecurityLevel', vec_size=512)
                with prog:
                    x = Input('x')
                    Output('y', 5*x*x + 3*x + x<<12 + 10)

                prog.set_output_ranges(20)
                prog.set_input_scales(30)

                self.assert_compiles_and_matches_reference(prog,
                    config={'security_level':s, 'quantum_safe':q, 'warn_vec_size':'false'})
    
    @unittest.expectedFailure
    def test_unsupported_security_level(self):
        """ Check that unsupported security levels error out """

        prog = EvaProgram('SecurityLevel', vec_size=512)
        with prog:
            x = Input('x')
            Output('y', 5*x*x + 3*x + x<<12 + 10)

        prog.set_output_ranges(20)
        prog.set_input_scales(30)

        self.assert_compiles_and_matches_reference(prog,
            config={'security_level':'1024','warn_vec_size':'false'})

    def test_reduction_balancer(self):
        """ Check that reductions are balanced under balance_reductions=true """

        prog = EvaProgram('ReductionTree', vec_size=16384)
        with prog:
            x1 = Input('x1')
            x2 = Input('x2')
            x3 = Input('x3')
            x4 = Input('x4')
            Output('y', (x1*(x2*(x3*x4))) + (x1+(x2+(x3+x4))))

        prog.set_output_ranges(20)
        prog.set_input_scales(60)
        
        progc, params, signature = self.assert_compiles_and_matches_reference(prog,
            config={'rescaler':'always', 'balance_reductions':'false', 'warn_vec_size':'false'})
        self.assertEqual(params.prime_bits, [60, 20, 60, 60, 60, 60])
        
        progc, params, signature = self.assert_compiles_and_matches_reference(prog,
            config={'rescaler':'always', 'balance_reductions':'true', 'warn_vec_size':'false'})
        self.assertEqual(params.prime_bits, [60, 20, 60, 60, 60])

    def test_seal_no_throw_on_transparent(self):
        """ Check that SEAL is compiled with -DSEAL_THROW_ON_TRANSPARENT_CIPHERTEXT=OFF
        
            An HE compiler cannot in general work with transparent ciphertext detection
            turned on because it is not possible to statically detect all situations that
            result in them. For example, x1-x2 is transparent only if the user gives the
            same ciphertext as both inputs."""

        prog = EvaProgram('Transparent', vec_size=4096)
        with prog:
            x = Input('x')
            Output('y', x-x+x*0)

        prog.set_output_ranges(20)
        prog.set_input_scales(30)

        self.assert_compiles_and_matches_reference(prog,
            config={'warn_vec_size':'false'})
    
    def test_serialization(self):
        """ Test (de)serialization and check that results stay the same """

        poly = EvaProgram('Polynomial', vec_size=4096)
        with poly:
            x = Input('x')
            Output('y', 3*x**2 + 5*x - 2)

        poly.set_output_ranges(20)
        poly.set_input_scales(30)

        inputs = {
            'x': [i for i in range(poly.vec_size)]
        }
        reference = evaluate(poly, inputs)

        compiler = CKKSCompiler(config={'warn_vec_size':'false'})
        poly, params, signature = compiler.compile(poly)

        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = lambda x: os.path.join(tmp_dir, x)

            save(poly, tmp_path('poly.eva'))
            save(params, tmp_path('poly.evaparams'))
            save(signature, tmp_path('poly.evasignature'))

            # Key generation time

            params = load(tmp_path('poly.evaparams'))

            public_ctx, secret_ctx = generate_keys(params)

            save(public_ctx, tmp_path('poly.sealpublic'))
            save(secret_ctx, tmp_path('poly.sealsecret'))

            # Runtime on client

            signature = load(tmp_path('poly.evasignature'))
            public_ctx = load(tmp_path('poly.sealpublic'))

            encInputs = public_ctx.encrypt(inputs, signature)

            save(encInputs, tmp_path('poly_inputs.sealvals'))

            # Runtime on server

            poly = load(tmp_path('poly.eva'))
            public_ctx = load(tmp_path('poly.sealpublic'))
            encInputs = load(tmp_path('poly_inputs.sealvals'))

            encOutputs = public_ctx.execute(poly, encInputs)

            save(encOutputs, tmp_path('poly_outputs.sealvals'))

            # Runtime back on client

            secret_ctx = load(tmp_path('poly.sealsecret'))
            encOutputs = load(tmp_path('poly_outputs.sealvals'))

        outputs = secret_ctx.decrypt(encOutputs, signature)

        reference_compiled = evaluate(poly, inputs)
        self.assertTrue(valuation_mse(reference, reference_compiled) < 0.0000000001)
        self.assertTrue(valuation_mse(outputs, reference) < 0.01)

if __name__ == '__main__':
    unittest.main()