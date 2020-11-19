# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

from eva import EvaProgram, Input, Output, evaluate, save, load
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
from eva.metric import valuation_mse
import numpy as np

#################################################
print('Compile time')

poly = EvaProgram('Polynomial', vec_size=8)
with poly:
    x = Input('x')
    Output('y', 3*x**2 + 5*x - 2)

poly.set_output_ranges(20)
poly.set_input_scales(20)

compiler = CKKSCompiler()
poly, params, signature = compiler.compile(poly)

save(poly, 'poly.eva')
save(params, 'poly.evaparams')
save(signature, 'poly.evasignature')

#################################################
print('Key generation time')

params = load('poly.evaparams')

public_ctx, secret_ctx = generate_keys(params)

save(public_ctx, 'poly.sealpublic')
save(secret_ctx, 'poly.sealsecret')

#################################################
print('Runtime on client')

signature = load('poly.evasignature')
public_ctx = load('poly.sealpublic')

inputs = {
    'x': [i for i in range(signature.vec_size)]
}
encInputs = public_ctx.encrypt(inputs, signature)

save(encInputs, 'poly_inputs.sealvals')

#################################################
print('Runtime on server')

poly = load('poly.eva')
public_ctx = load('poly.sealpublic')
encInputs = load('poly_inputs.sealvals')

encOutputs = public_ctx.execute(poly, encInputs)

save(encOutputs, 'poly_outputs.sealvals')

#################################################
print('Back on client')

secret_ctx = load('poly.sealsecret')
encOutputs = load('poly_outputs.sealvals')

outputs = secret_ctx.decrypt(encOutputs, signature)

reference = evaluate(poly, inputs)
print('Expected', reference)
print('Got', outputs)
print('MSE', valuation_mse(outputs, reference))
