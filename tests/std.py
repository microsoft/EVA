# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

import unittest
from common import *
from eva import EvaProgram, Input, Output
from eva.std.numeric import horizontal_sum

class Std(EvaTestCase):
    def test_horizontal_sum(self):
        """ Test eva.std.numeric.horizontal_sum """

        for enc in [True, False]:
            prog = EvaProgram('HorizontalSum', vec_size = 2048)
            with prog:
                x = Input('x', is_encrypted=enc)
                y = horizontal_sum(x)
                Output('y', y)

            prog.set_output_ranges(25)
            prog.set_input_scales(33)

            self.assert_compiles_and_matches_reference(prog,
                config={'warn_vec_size':'false'})

        prog = EvaProgram('HorizontalSumConstant', vec_size = 2048)
        with prog:
            y = horizontal_sum([1 for _ in range(prog.vec_size)])
            Output('y', y)

        prog.set_output_ranges(25)
        prog.set_input_scales(33)

        self.assert_compiles_and_matches_reference(prog,
            config={'warn_vec_size':'false'})

if __name__ == '__main__':
    unittest.main()