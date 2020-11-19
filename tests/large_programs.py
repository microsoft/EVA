# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

import unittest
import math
from common import *
from eva import EvaProgram, Input, Output

class LargePrograms(EvaTestCase):
    def test_sobel_configs(self):
        """ Check accuracy of Sobel filter on random image with various compiler configurations """

        def convolutionXY(image, width, filter):
            for i in range(len(filter)):
                for j in range(len(filter[0])):
                    rotated = image << (i * width + j)
                    horizontal = rotated * filter[i][j]
                    vertical = rotated * filter[j][i]
                    if i == 0 and j == 0:
                        Ix = horizontal
                        Iy = vertical
                    else:
                        Ix += horizontal
                        Iy += vertical
            return Ix, Iy

        h = 90
        w = 90

        sobel = EvaProgram('sobel', vec_size=2**(math.ceil(math.log(h*w, 2))))
        with sobel:
            image = Input('image')

            sobel_filter = [
                [-1, 0, 1],
                [-2, 0, 2],
                [-1, 0, 1]]

            a1 = 2.2137874823876622
            a2 = -1.0984324107372518
            a3 = 0.17254603006834726

            conv_hor, conv_ver = convolutionXY(image, w, sobel_filter)
            x = conv_hor**2 + conv_ver**2
            Output('image', x * a1 + x**2 * a2 + x**3 * a3)

        sobel.set_input_scales(45)
        sobel.set_output_ranges(20)

        for rescaler in ['lazy_waterline','eager_waterline','always']:
            for balance_reductions in ['true','false']:
                self.assert_compiles_and_matches_reference(sobel,
                    config={'rescaler':rescaler,'balance_reductions':balance_reductions})

    def test_regression(self):
        """ Test batched compilation and execution of multiple linear regression programs """
        
        linreg = EvaProgram('linear_regression', vec_size=2048)
        with linreg:
            p = 63

            x = [Input(f'x{i}') for i in range(p)]
            e = Input('e')
            b0 = 6.56
            b = [i * 0.732 for i in range(p)]

            y = e + b0
            for i in range(p):
                t = x[i] * b[i]
                y += t

            Output('y', y)

        linreg.set_input_scales(40)
        linreg.set_output_ranges(30)

        linreg_inputs = {'e': [(linreg.vec_size - i) * 0.001 for i in range(linreg.vec_size)]}
        for i in range(p):
            linreg_inputs[f'x{i}'] = [i * j * 0.01 for j in range(linreg.vec_size)]

        polyreg = EvaProgram('polynomial_regression', vec_size=4096)
        with polyreg:
            p = 4

            x = Input('x')
            e = Input('e')
            b0 = 6.56
            b = [i * 0.732 for i in range(p)]

            y = e + b0
            for i in range(p):
                x_i = x
                for j in range(i):
                    x_i = x_i * x
                t = x_i * b[i]
                y += t

            Output('y', y)

        polyreg.set_input_scales(40)
        polyreg.set_output_ranges(30)

        polyreg_inputs = {
            'x': [i * 0.01 for i in range(polyreg.vec_size)],
            'e': [(polyreg.vec_size - i) * 0.001 for i in range(polyreg.vec_size)],
        }

        multireg = EvaProgram('multivariate_regression', vec_size=2048)
        with multireg:
            p = 63
            k = 4

            x = [Input(f'x{i}') for i in range(p)]
            e = [Input(f'e{j}') for j in range(k)]
            b0 = [j * 0.56 for j in range(k)]
            b = [[k * i * 0.732 for i in range(p)] for j in range(k)]

            y = [0 for j in range(k)]
            for j in range(k):
                y[j] = e[j] + b0[j]
                for i in range(p):
                    t = x[i] * b[j][i]
                    y[j] += t

            for j in range(k):
                Output(f'y{j}', y[j])

        multireg.set_input_scales(40)
        multireg.set_output_ranges(30)

        multireg_inputs = {}
        for i in range(p):
            multireg_inputs[f'x{i}'] = [i * j * 0.01 for j in range(multireg.vec_size)]
        for j in range(k):
            multireg_inputs[f'e{j}'] = [(multireg.vec_size - i) * j * 0.001 for i in range(multireg.vec_size)]
            
        compiler = CKKSCompiler(config={'warn_vec_size':'false'})

        for prog, inputs in [(linreg, linreg_inputs), (polyreg, polyreg_inputs), (multireg, multireg_inputs)]:
            compiled_prog, params, signature = compiler.compile(prog)
            public_ctx, secret_ctx = generate_keys(params)
            enc_inputs = public_ctx.encrypt(inputs, signature)
            enc_outputs = public_ctx.execute(compiled_prog, enc_inputs)
            outputs = secret_ctx.decrypt(enc_outputs, signature)
            reference = evaluate(compiled_prog, inputs)
            self.assertTrue(valuation_mse(outputs, reference) < 0.01)

if __name__ == '__main__':
    unittest.main()