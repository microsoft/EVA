# EVA - Compiler for Microsoft SEAL

EVA is a compiler for homomorphic encryption, that automates away the parts that require cryptographic expertise.
This gives you a simple way to write programs that operate on encrypted data without having access to the secret key.

Think of EVA as the "C compiler" of the homomorphic world. Homomorphic computations written in EVA IR (Encrypted Vector Arithmetic Intermediate Representation) get compiled to the "assembly" of the homomorphic encryption library API. Just like C compilers free you from tricky tasks like register allocation, EVA frees you from *encryption parameter selection, rescaling insertion, <small>relinearization</small>*...

EVA targets [Microsoft SEAL](https://github.com/microsoft/SEAL) — the industry leading library for fully-homomorphic encryption — and currently supports the CKKS scheme for deep computations on encrypted approximate fixed-point arithmetic.

## Getting Started

EVA is a native library written in C++17 with bindings for Python. Both Linux and Windows are supported. The instructions below show how to get started with EVA on Ubuntu. For building on Windows [EVA's Azure Pipelines script](azure-pipelines.yml) is a useful reference.

### Installing Dependencies

To install dependencies on Ubuntu 20.04:
```
sudo apt install cmake libboost-all-dev libprotobuf-dev protobuf-compiler
```

Clang is recommended for compilation, as SEAL is faster when compiled with it. To install clang and set it as default:
```
sudo apt install clang
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100
```

Next install Microsoft SEAL version 3.6:
```
git clone -b v3.6.4 https://github.com/microsoft/SEAL.git
cd SEAL
cmake -DSEAL_THROW_ON_TRANSPARENT_CIPHERTEXT=OFF .
make -j
sudo make install
```
*Note that SEAL has to be installed with transparent ciphertext checking turned off, as it is not possible in general to statically ensure a program will not produce a transparent ciphertext. This does not affect the security of ciphertexts encrypted with SEAL.*

### Building and Installing EVA

#### Building EVA

EVA builds with CMake version ≥ 3.13:
```
git submodule update --init
cmake .
make -j
```
The build process creates a `setup.py` file in `python/`. To install the package for development with PIP:
```
python3 -m pip install -e python/
```
To create a Python Wheel package for distribution in `dist/`:
```
python3 python/setup.py bdist_wheel --dist-dir='.'
```

To check that the installed Python package is working correctly, run all tests with:
```
python3 tests/all.py
``` 

EVA does not yet support installing the native library for use in other CMake projects (contributions very welcome).

#### Multicore Support

EVA features highly scalable multicore support using the [Galois library](https://github.com/IntelligentSoftwareSystems/Galois). It is included as a submodule, but is turned off by default for faster builds and easier debugging. To build EVA with Galois configure with `USE_GALOIS=ON`:
```
cmake -DUSE_GALOIS=ON .
```

### Running the Examples

The examples use EVA's Python APIs. To install dependencies with PIP:
```
python3 -m pip install -r examples/requirements.txt
```

To run for example the image processing example in EVA/examples:
```
cd examples/
python3 image_processing.py
```
This will compile and run homomorphic evaluations of a Sobel edge detection filter and a Harris corner detection filter on `examples/baboon.png`, producing results of homomorphic evaluation in `*_encrypted.png` and reference results from normal execution in `*_reference.png`.
The script also reports the mean squared error between these for each filter.

## Programming with PyEVA

PyEVA is a thin Python-embedded DSL for producing EVA programs.
We will walk you through compiling a PyEVA program with EVA and running it on top of SEAL.

### Writing and Compiling Programs

A program to evaluate a fixed polynomial 3x<sup>2</sup>+5x-2 on 1024 encrypted values can be written:
```
from eva import *
poly = EvaProgram('Polynomial', vec_size=1024)
with poly:
    x = Input('x')
    Output('y', 3*x**2 + 5*x - 2)
```
Next we will compile this program for the [CKKS encryption scheme](https://eprint.iacr.org/2016/421.pdf).
Two additional pieces of information EVA currently requires to compile for CKKS are the *fixed-point scale for inputs* and the *maximum ranges of coefficients in outputs*, both represented in number of bits:
```
poly.set_output_ranges(30)
poly.set_input_scales(30)
```
Now the program can be compiled:
```
from eva.ckks import *
compiler = CKKSCompiler()
compiled_poly, params, signature = compiler.compile(poly)
```
The `compile` method transforms the program in-place and returns:

1. the compiled program;
2. encryption parameters for Microsoft SEAL with which the program can be executed;
3. a signature object, that specifies how inputs and outputs need to be encoded and decoded.

The compiled program can be inspected by printing it in the DOT format for the [Graphviz](https://graphviz.org/) visualization software:
```
print(compiled_poly.to_DOT())
```
The output can be viewed as a graph in, for example, a number of Graphviz editors available online.

### Generating Keys and Encrypting Inputs

Encryption keys can now be generated using the encryption parameters:
```
from eva.seal import *
public_ctx, secret_ctx = generate_keys(params)
```
Next a dictionary of inputs is created and encrypted using the public context and the program signature:
```
inputs = { 'x': [i for i in range(compiled_poly.vec_size)] }
encInputs = public_ctx.encrypt(inputs, signature)
```

### Homomorphic Execution

Everything is now in place for executing the program with Microsoft SEAL:
```
encOutputs = public_ctx.execute(compiled_poly, encInputs)
```

### Decrypting Results

Finally, the outputs can be decrypted using the secret context:
```
outputs = secret_ctx.decrypt(encOutputs, signature)
```
For debugging it is often useful to compare homomorphic results to unencrypted computation.
The `evaluate` method can be used to execute an EVA program on unencrypted data.
The two sets of results can then be compared with for example Mean Squared Error:

```
from eva.metric import valuation_mse
reference = evaluate(compiled_poly, inputs)
print('MSE', valuation_mse(outputs, reference))
```

## Contributing

The EVA project welcomes contributions and suggestions. Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## Credits

This project is a collaboration between the Microsoft Research's Research in Software Engineering (RiSE) group and Cryptography and Privacy Research group.

A huge credit goes to [Dr. Roshan Dathathri](https://roshandathathri.github.io/), who as an intern built the first version of EVA, along with all the transformations required for targeting the CKKS scheme efficiently and the parallelizing runtime required to make execution scale.

Many thanks to [Sangeeta Chowdhary](https://www.ilab.cs.rutgers.edu/~sc1696/), who as an intern put a huge amount of work into making EVA ready for release.

## Publications

Roshan Dathathri, Blagovesta Kostova, Olli Saarikivi, Wei Dai, Kim Laine, Madanlal Musuvathi. *EVA: An Encrypted Vector Arithmetic Language and Compiler for Efficient Homomorphic Computation*. PLDI 2020. [arXiv](https://arxiv.org/abs/1912.11951) [DOI](https://doi.org/10.1145/3385412.3386023)

Roshan Dathathri, Olli Saarikivi, Hao Chen, Kim Laine, Kristin Lauter, Saeed Maleki, Madanlal Musuvathi, Todd Mytkowicz. *CHET: An Optimizing Compiler for Fully-Homomorphic Neural-Network Inferencing*. PLDI 2019. [DOI](https://doi.org/10.1145/3314221.3314628)
