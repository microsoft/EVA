# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.
# In this tutorial we compute the secure evaluation of sign function
# Given the encryption of x, Enc(x) the program computes the encryption of
# the sign of x, Enc(sgn(x)).
# Given the fact that we cannot compute directly the homomorphic evaluation of the sign function,
# we must approximate the sign function by a polynomial and then use
# the homomorphic properties of CKKS to evaluate that polynomial
# This is the implementation of the https://arxiv.org/pdf/1810.12380.pdf

from eva import EvaProgram, Input, Output, evaluate
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
from eva.metric import valuation_mse
import random
from matplotlib import pyplot as plt
import numpy as np

num = 16384
random.seed(32)
def generate_data():
    x = list(np.random.uniform(low=-0.12, high=0.12, size=(num)))
    one = [1] * num
    const = [1.9142] * num
    return {'x': x,'one': one, 'const': const}

#The function approximates the tanh limit
def aprox_limit(x, r, one, const):
    for idx in range(r):
        x = x * (const - (x**2))
    return (x+one)*0.5

#The function computes the encrypted sign of x
def compute_sign(x, r, one, const):
    lim = aprox_limit(x, r, one, const)
    lim = lim*2 - one
    return lim

#The function computes the real (non-approximate) sign of x
def determine_sign(data):
    x = data['x']
    sign = []
    for idx in range(num):
        if x[idx] > 0:
            sign.append(1)
        elif x[idx] < 0:
            sign.append(-1)
        else:
            sign.append(0)

    return {'sign': sign}

secure_comparison = EvaProgram('secure_comparison', vec_size=num)
with secure_comparison:
    x = Input('x')
    one = Input('one')
    const = Input('const')
    r = 5
    sign = compute_sign(x, r, one, const)
    Output('sign', sign)
secure_comparison.set_input_scales(40)
secure_comparison.set_output_ranges(30)


def compile_and_run(inputs, prog):
    print(f'Compiling {prog.name}')
    compiler = CKKSCompiler()
    compiled, params, signature = compiler.compile(prog)
    public_ctx, secret_ctx = generate_keys(params)
    enc_inputs = public_ctx.encrypt(inputs, signature)
    enc_outputs = public_ctx.execute(compiled, enc_inputs)
    outputs = secret_ctx.decrypt(enc_outputs, signature)
    reference = evaluate(compiled, inputs)
    return outputs, reference

#The function displays the error between the approximate sign and the real sign
def display_error(inputs, outputs):
    non_aprox_sign = determine_sign(inputs)
    aprox_sign = outputs['sign']
    dif = [(non_aprox_sign['sign'][idx] - aprox_sign[idx])**2 for idx in range(num)]
    plt.scatter(inputs['x'], dif)
    plt.title(str(valuation_mse(outputs, non_aprox_sign)))
    plt.show()


inputs = generate_data()
prog = secure_comparison
outputs, reference = compile_and_run(inputs, prog)
print('MSE outputs-aprox', valuation_mse(outputs, reference))
display_error(inputs, outputs)
