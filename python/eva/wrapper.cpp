// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "eva/eva.h"

namespace py = pybind11;
using namespace eva;
using namespace std;

const char* const SAVE_DOC_STRING = R"DELIMITER(Serialize and save an EVA object to a file.

Parameters
----------
path : str
    Path of the file to save to
)DELIMITER";

// clang-format off
PYBIND11_MODULE(_eva, m) {
  m.doc() = "Python wrapper for EVA";
  m.attr("__name__") = "eva._eva";

  py::enum_<Op>(m, "Op")
#define X(op,code) .value(#op, Op::op)
EVA_OPS
#undef X
  ;
  py::enum_<Type>(m, "Type")
#define X(type,code) .value(#type, Type::type)
EVA_TYPES
#undef X
  ;
  py::class_<Term, shared_ptr<Term>>(m, "Term", "EVA's native Term class")
    .def_readonly("op", &Term::op, "The operation performed by this term");
  py::class_<Program>(m, "Program", "EVA's native Program class")
    .def(py::init<string,uint64_t>(), py::arg("name"), py::arg("vec_size"))
    .def_property("name", &Program::getName, &Program::setName, "The name of this program")
    .def_property_readonly("vec_size", &Program::getVecSize, "The number of elements for all vectors in this program")
    .def_property_readonly("inputs", &Program::getInputs, py::keep_alive<0,1>(), "A dictionary from input names to terms")
    .def_property_readonly("outputs", &Program::getOutputs, py::keep_alive<0,1>(), "A dictionary from output names to terms")
    .def("set_output_ranges", [](const Program& prog, uint32_t range) {
      for (auto& entry : prog.getOutputs()) {
        entry.second->set<RangeAttribute>(range);
      }
    }, R"DELIMITER(Affects the ranges of output that the program must accomodate. Sets all
outputs at once.

The value given here does not directly translate to a supported range of
values, as this only ensures the ranges that coefficients may take in 
CKKS's encoded form. Some patterns of values may result in coefficients
that are larger than any of the values themselves. If you see overflow
increasing the value given here will help.

Parameters
----------
range : int
    The range in bits. Must be positive.)DELIMITER", py::arg("range"))
    .def("set_input_scales", [](const Program& prog, uint32_t scale) {
      for (auto& source : prog.getSources()) {
        source->set<EncodeAtScaleAttribute>(scale);
      }
    }, R"DELIMITER(Sets the scales that inputs will be encoded at. Sets the scales for all
inputs at once. This value will also be interpreted as the minimum scale
that any intermediate value have.

Parameters
----------
scale : int
    The scale in bits. Must be positive.)DELIMITER", py::arg("scale"))
    .def("to_DOT", &Program::toDOT, R"DELIMITER(Produce a graph representation of the program in the DOT format.

Returns
-------
str
    The graph in DOT format)DELIMITER")
    .def("_make_term", &Program::makeTerm, py::keep_alive<0,1>())
    .def("_make_left_rotation", &Program::makeLeftRotation, py::keep_alive<0,1>())
    .def("_make_right_rotation", &Program::makeRightRotation, py::keep_alive<0,1>())
    .def("_make_dense_constant", &Program::makeDenseConstant, py::keep_alive<0,1>())
    .def("_make_uniform_constant", &Program::makeUniformConstant, py::keep_alive<0,1>())
    .def("_make_input", &Program::makeInput, py::keep_alive<0,1>())
    .def("_make_output", &Program::makeOutput, py::keep_alive<0,1>());

  m.def("evaluate", &evaluate, R"DELIMITER(Evaluate the program without homomorphic encryption

This function implements the reference semantics of EVA. During your
development process you may check that homomorphic evaluation is
giving results that match the unencrypted evaluation given by this function.

Parameters
----------
program : Program
    The program to be evaluated
inputs : dict from strings to lists of numbers
    The inputs for the evaluation

Returns
-------
dict from strings to lists of numbers
    The outputs from the evaluation)DELIMITER", py::arg("program"), py::arg("inputs"));
  
  // Serialization
  m.def("save", &saveToFile<Program>, SAVE_DOC_STRING, py::arg("obj"), py::arg("path"));
  m.def("save", &saveToFile<CKKSParameters>, SAVE_DOC_STRING, py::arg("obj"), py::arg("path"));
  m.def("save", &saveToFile<CKKSSignature>, SAVE_DOC_STRING, py::arg("obj"), py::arg("path"));
  m.def("save", &saveToFile<SEALValuation>, SAVE_DOC_STRING, py::arg("obj"), py::arg("path"));
  m.def("save", &saveToFile<SEALPublic>, SAVE_DOC_STRING, py::arg("obj"), py::arg("path"));
  m.def("save", &saveToFile<SEALSecret>, SAVE_DOC_STRING, py::arg("obj"), py::arg("path"));
  m.def("load", static_cast<KnownType (*)(const string&)>(&loadFromFile), R"DELIMITER(Load and deserialize a previously serialized EVA object from a file.

Parameters
----------
path : str
    Path of the file to load from

Returns
-------
An object of the same class as was previously serialized)DELIMITER", py::arg("path"));

  // CKKS compiler
  py::module mckks = m.def_submodule("_ckks", "Python wrapper for EVA CKKS compiler");
  py::class_<CKKSCompiler>(mckks, "CKKSCompiler")
    .def(py::init(), "Create a compiler with the default config")
    .def(py::init<std::unordered_map<std::string,std::string>>(), R"DELIMITER(Create a compiler with a custom config

Parameters
----------
config : dict from strings to strings
    The configuration options to override)DELIMITER", py::arg("config"))
    .def("compile", &CKKSCompiler::compile, R"DELIMITER(Compile a program for CKKS

Parameters
----------
program : Program
    The program to compile

Returns
-------
Program
    The compiled program
CKKSParameters
    The selected encryption parameters
CKKSSignature
    The signature of the program)DELIMITER", py::arg("program"));
  py::class_<CKKSParameters>(mckks, "CKKSParameters", "Abstract encryption parameters for CKKS")
    .def_readonly("prime_bits", &CKKSParameters::primeBits, "List of number of bits each prime should have")
    .def_readonly("rotations", &CKKSParameters::rotations, "List of steps that rotation keys should be generated for")
    .def_readonly("poly_modulus_degree", &CKKSParameters::polyModulusDegree, "The polynomial degree N required");
  py::class_<CKKSSignature>(mckks, "CKKSSignature", "The signature of a compiled program used for encoding and decoding")
    .def_readonly("vec_size", &CKKSSignature::vecSize, "The vector size of the program")
    .def_readonly("inputs", &CKKSSignature::inputs, "Dictionary of CKKSEncodingInfo objects for each input");
  py::class_<CKKSEncodingInfo>(mckks, "CKKSEncodingInfo", "Holds the information required for encoding an input")
    .def_readonly("input_type", &CKKSEncodingInfo::inputType, "The type of this input. Decides whether input is encoded, also encrypted or neither.")
    .def_readonly("scale", &CKKSEncodingInfo::scale, "The scale encoding should happen at")
    .def_readonly("level", &CKKSEncodingInfo::level, "The level encoding should happen at");

  // SEAL backend
  py::module mseal = m.def_submodule("_seal", "Python wrapper for EVA SEAL backend");
  mseal.def("generate_keys", &generateKeys, R"DELIMITER(Generate keys required for evaluation with SEAL

Parameters
----------
abstract_params : CKKSParameters
    Specification of the encryption parameters from the compiler

Returns
-------
SEALPublic
    The public part of the SEAL context that is used for encryption and execution.
SEALSecret
    The secret part of the SEAL context that is used for decryption.
    WARNING: This object holds your generated secret key. Do not share this object
              (or its serialized form) with anyone you do not want having access
              to the values encrypted with the public context.)DELIMITER", py::arg("absract_params"));
  py::class_<SEALValuation>(mseal, "SEALValuation", "A valuation for inputs or outputs holding values encrypted with SEAL");
  py::class_<SEALPublic>(mseal, "SEALPublic", "The public part of the SEAL context that is used for encryption and execution.")
    .def("encrypt", &SEALPublic::encrypt, R"DELIMITER(Encrypt inputs for a compiled EVA program

Parameters
----------
inputs : dict from strings to lists of numbers
    The values to be encrypted
signature : CKKSSignature
    The signature of the program the inputs are being encrypted for

Returns
-------
SEALValuation
    The encrypted inputs)DELIMITER", py::arg("inputs"), py::arg("signature"))
    .def("execute", &SEALPublic::execute, R"DELIMITER(Execute a compiled EVA program with SEAL

Parameters
----------
program : Program
    The program to be executed
inputs : SEALValuation
    The encrypted valuation for the inputs of the program

Returns
-------
SEALValuation
    The encrypted outputs)DELIMITER", py::arg("program"), py::arg("inputs"));
  py::class_<SEALSecret>(mseal, "SEALSecret", R"DELIMITER(The secret part of the SEAL context that is used for decryption.

WARNING: This object holds your generated secret key. Do not share this object
          (or its serialized form) with anyone you do not want having access
          to the values encrypted with the public context.)DELIMITER")
    .def("decrypt", &SEALSecret::decrypt, R"DELIMITER(Decrypt outputs from a compiled EVA program

Parameters
----------
enc_outputs : SEALValuation
    The values to be decrypted
signature : CKKSSignature
    The signature of the program the outputs are being decrypted for

Returns
-------
dict from strings to lists of numbers
    The decrypted outputs)DELIMITER", py::arg("enc_outputs"), py::arg("signature"));
}
// clang-format on
