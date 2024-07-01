from eva import EvaProgram, Input, Output
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
from eva.metric import valuation_mse
from eva import evaluate

# A program to evaluate a fixed polynomial 3x^2 + 5x - 2 on 1024 encrypted values
poly = EvaProgram('Polynomial', vec_size=1024)
with poly:
    x = Input('x')
    Output('y', 3*x**2 + 5*x - 2)

# Set the output ranges and input scales to ensure precision during homomorphic operations
poly.set_output_ranges(30)
poly.set_input_scales(30)

# Compile the program using the CKKS compiler
compiler = CKKSCompiler()
compiled_poly, params, signature = compiler.compile(poly)

# Print the compiled program in DOT format for visualization
print(compiled_poly.to_DOT())

# Generate the public and secret keys for encryption and decryption
public_ctx, secret_ctx = generate_keys(params)
# print(f"public_ctx: {public_ctx}")
# print(f"secret_ctx: {secret_ctx}")

# Create and encrypt the input values
inputs = { 'x': [i for i in range(compiled_poly.vec_size)] }
# print(f"inputs: {inputs}")
encInputs = public_ctx.encrypt(inputs, signature)
# print(f"encInputs: {encInputs}")

# Perform homomorphic evaluation on the encrypted inputs
encOutputs = public_ctx.execute(compiled_poly, encInputs)
# print(f"encOutputs: {encOutputs}")

# Decrypt the output values
outputs = secret_ctx.decrypt(encOutputs, signature)
# print(f"outputs: {outputs}")

# Evaluate the polynomial on plaintext inputs for comparison
reference = evaluate(compiled_poly, inputs)
print('MSE', valuation_mse(outputs, reference))
