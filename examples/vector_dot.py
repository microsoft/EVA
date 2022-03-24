#!/usr/bin/env python
import argparse
from eva import EvaProgram, Input, Output
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
import numpy as np
import time
from eva.std.numeric import horizontal_sum


def dot(x, y):
    return np.dot(x, y)


def generate_inputs_naive(size, label="x"):
    inputs = dict()
    inputs_np = np.zeros((size))
    i = 0
    for n in range(size):
        # each element is a list (i.e. a vector of size 1)
        inputs[f"{label}_{n}"] = [i]
        inputs_np[n] = i
        i += 1
    return inputs, inputs_np


def generate_vector_dot_naive(size):
    """Vector dot product with vector size of 1"""
    fhe_dot = EvaProgram("fhe_dot", vec_size=1)
    with fhe_dot:
        a = np.array([Input(f"x_{n}") for n in range(size)]).reshape(1, size)
        b = np.array([Input(f"w_{k}") for k in range(size)]).reshape(size, 1)

        out = dot(a, b)

        Output("y", out[0][0])

    fhe_dot.set_input_scales(32)
    fhe_dot.set_output_ranges(32)
    return fhe_dot


def generate_inputs(size, label="x"):
    inputs = dict()
    inputs_np = np.zeros((size))
    i = 0
    # all data is stored in a single list of size `size`
    inputs[label] = list(range(size))
    for n in range(size):
        inputs_np[n] = i
        i += 1
    return inputs, inputs_np


def generate_vector_dot(size):
    """Vector dot product with CKKS vector size equal to the size"""
    fhe_dot = EvaProgram("fhe_dot", vec_size=size)
    with fhe_dot:
        a = np.array([Input("x")])
        b = np.array([Input(f"w")])

        out = dot(a, b)

        Output("y", horizontal_sum(out))

    fhe_dot.set_input_scales(32)
    fhe_dot.set_output_ranges(32)
    return fhe_dot


def benchmark_vector_dot(size, mode="SIMD"):
    if mode == "SIMD":
        # generate program with SIMD-style
        inputs, inputs_np = generate_inputs(size, label="x")
        weights, weights_np = generate_inputs(size, label="w")
        fhe_dot = generate_vector_dot(size)
    else:
        # generate program with vector size = 1
        inputs, inputs_np = generate_inputs_naive(size, label="x")
        weights, weights_np = generate_inputs_naive(size, label="w")
        fhe_dot = generate_vector_dot_naive(size)

    # compiling program
    data = {**weights, **inputs}
    compiler = CKKSCompiler(config={"security_level": "128", "warn_vec_size": "false"})
    compiled, params, signature = compiler.compile(fhe_dot)
    public_ctx, secret_ctx = generate_keys(params)
    enc_inputs = public_ctx.encrypt(data, signature)

    # Running program
    start = time.time()
    enc_outputs = public_ctx.execute(compiled, enc_inputs)
    end = time.time()
    run_time = end - start

    # decrypt the output
    outputs = secret_ctx.decrypt(enc_outputs, signature)
    y = np.array(outputs["y"])

    # get time for plaintext dot product
    start = time.time()
    true_y = inputs_np.dot(weights_np)
    end = time.time()
    plain_run_time = end - start

    # verifying correctness of output
    np.testing.assert_allclose(y, true_y)

    return run_time, plain_run_time


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run a dot product program")
    parser.add_argument(
        "--mode",
        default="SIMD",
        choices=["SIMD", "naive"],
    )
    args = parser.parse_args()
    results_cipher = dict()
    results_plain = dict()
    if args.mode == "SIMD":
        print("Generating code in SIMD style")
    else:
        print("Generating code in naive style")

    for size in [4, 8, 16, 32, 64, 128, 256, 512, 1024]:
        time_cipher, time_plain = benchmark_vector_dot(size, args.mode)

        results_cipher[f"{size}"] = time_cipher
        results_plain[f"{size}"] = time_plain
        print(f"Done vector size {size}, CKKS time: {time_cipher}")
    print("Done")
    print("CKKS times:", results_cipher)
    print("Plain text times:", results_plain)
