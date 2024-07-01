from eva import EvaProgram, Input, Output
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
from eva.metric import valuation_mse
from eva import evaluate

# A program to perform basic operations on encrypted latitude and longitude values
geo_transform = EvaProgram('GeoTransform', vec_size=1024)
with geo_transform:
    lat = Input('lat')
    lon = Input('lon')
    lat_plus_lon = lat + lon
    lat_minus_lon = lat - lon
    Output('lat_plus_lon', lat_plus_lon)
    Output('lat_minus_lon', lat_minus_lon)

# Set the output ranges and input scales to ensure precision during homomorphic operations
geo_transform.set_output_ranges(30)
geo_transform.set_input_scales(30)

# Compile the program using the CKKS compiler
compiler = CKKSCompiler()
compiled_geo_transform, params, signature = compiler.compile(geo_transform)

# Print the compiled program in DOT format for visualization
print(compiled_geo_transform.to_DOT())

# Generate the public and secret keys for encryption and decryption
public_ctx, secret_ctx = generate_keys(params)

# Create and encrypt the input values (example latitude and longitude values)
inputs = {
    'lat': [i + 40.0 for i in range(compiled_geo_transform.vec_size)],  # example latitudes
    'lon': [i - 70.0 for i in range(compiled_geo_transform.vec_size)]   # example longitudes
}
encInputs = public_ctx.encrypt(inputs, signature)

# Perform homomorphic evaluation on the encrypted inputs
encOutputs = public_ctx.execute(compiled_geo_transform, encInputs)

# Decrypt the output values
outputs = secret_ctx.decrypt(encOutputs, signature)

# Evaluate the transformations on plaintext inputs for comparison
reference = evaluate(compiled_geo_transform, inputs)
print('MSE', valuation_mse(outputs, reference))
