# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

import unittest
from common import *
from eva import EvaProgram, Input, Output

class BugFixes(EvaTestCase):

    def test_high_inner_term_scale(self):
        """ Test lazy waterline rescaler with a program causing a high inner term scale

            This test was added for a bug that was an interaction between
            rescaling not being inserted (causing high scales to be accumulated)
            and parameter selection not handling high scales in inner terms."""

        prog = EvaProgram('HighInnerTermScale', vec_size=4)
        with prog:
            x1 = Input('x1')
            x2 = Input('x2')
            Output('y', x1*x1*x2)

        prog.set_output_ranges(20)
        prog.set_input_scales(60)
        
        self.assert_compiles_and_matches_reference(prog, config={'rescaler':'lazy_waterline'})

    @unittest.skip('not fixed in SEAL yet')
    def test_large_and_small(self):
        """ Check that a ciphertext with very large and small values decodes accurately
            
            This test was added to track a common bug in CKKS implementations,
            where double precision floating points used in decoding fail to
            provide good accuracy for small values in ciphertexts when other
            very large values are present."""

        prog = EvaProgram('LargeAndSmall', vec_size=4)
        with prog:
            x = Input('x')
            Output('y', pow(x,8))

        prog.set_output_ranges(60)
        prog.set_input_scales(60)

        inputs = {
            'x': [0,1,10,100]
        }

        self.assert_compiles_and_matches_reference(prog, inputs, config={'warn_vec_size':'false'})

    def test_output_rescaled(self):
        """ Check that the lazy waterline policy rescales outputs
        
            This test was added for a bug where outputs could be returned with
            more primes in their modulus than necessary, which causes them to
            take more space when serialized."""

        prog = EvaProgram('OutputRescaled', vec_size=4)
        with prog:
            x = Input('x')
            Output('y', x*x)

        prog.set_output_ranges(20)
        prog.set_input_scales(60)

        compiler = CKKSCompiler(config={'rescaler':'lazy_waterline', 'warn_vec_size':'false'})
        prog, params, signature = compiler.compile(prog)
        self.assertEqual(params.prime_bits, [60, 20, 60, 60])

if __name__ == '__main__':
    unittest.main()
