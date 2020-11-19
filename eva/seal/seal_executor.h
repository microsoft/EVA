// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ir/constant_value.h"
#include "eva/ir/program.h"
#include "eva/ir/term_map.h"
#include "eva/util/logging.h"
#include "eva/util/overloaded.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <seal/seal.h>
#include <stdexcept>
#include <variant>
#include <vector>

// Galois per-thread storage is used when EVA is compiled for multicore support
#ifdef EVA_USE_GALOIS
#include <galois/substrate/PerThreadStorage.h>
#endif

namespace eva {

// executes unencrypted computation
class SEALExecutor {
  using RuntimeValue =
      std::variant<seal::Ciphertext, seal::Plaintext, std::vector<double>>;

  Program &program;
  seal::SEALContext context;
  seal::CKKSEncoder &encoder;
  seal::Encryptor &encryptor;
  seal::Evaluator &evaluator;
  seal::GaloisKeys &galoisKeys;
  seal::RelinKeys &relinKeys;
  TermMapOptional<RuntimeValue> Objects;

  // Each thread has a separate scratch space into which constants are expanded
  // for encoding.
#ifdef EVA_USE_GALOIS
  galois::substrate::PerThreadStorage<std::vector<double>> tempVec;
#else
  // Without multicore support only one scratch vector is needed
  std::vector<double> tempVec;
#endif

  bool isCipher(const Term::Ptr &t) {
    return std::holds_alternative<seal::Ciphertext>(Objects.at(t));
  }
  bool isPlain(const Term::Ptr &t) {
    return std::holds_alternative<seal::Plaintext>(Objects.at(t));
  }
  bool isRaw(const Term::Ptr &t) {
    return std::holds_alternative<std::vector<double>>(Objects.at(t));
  }

  void rightRotateRaw(std::vector<double> &out, const Term::Ptr &args1,
                      std::int32_t shift) {
    auto &in = std::get<std::vector<double>>(Objects.at(args1));

    while (shift > 0 && shift >= in.size())
      shift -= in.size();
    while (shift < 0)
      shift += in.size();

    out.clear();
    out.reserve(in.size());
    copy_n(in.cend() - shift, shift, back_inserter(out));
    copy_n(in.cbegin(), in.size() - shift, back_inserter(out));
  }

  void leftRotateRaw(std::vector<double> &out, const Term::Ptr &args1,
                     std::int32_t shift) {
    auto &in = std::get<std::vector<double>>(Objects.at(args1));

    while (shift > 0 && shift >= in.size())
      shift -= in.size();
    while (shift < 0)
      shift += in.size();

    out.clear();
    out.reserve(in.size());
    copy_n(in.cbegin() + shift, in.size() - shift, back_inserter(out));
    copy_n(in.cbegin(), shift, back_inserter(out));
  }

  template <class Op>
  void binOpRaw(std::vector<double> &out, const Term::Ptr &args1,
                const Term::Ptr &args2) {
    auto &in1 = std::get<std::vector<double>>(Objects.at(args1));
    auto &in2 = std::get<std::vector<double>>(Objects.at(args2));
    assert(in1.size() == in2.size());

    out.clear();
    out.reserve(in1.size());
    transform(in1.cbegin(), in1.cend(), in2.cbegin(), back_inserter(out), Op());
  }

  void negateRaw(std::vector<double> &out, const Term::Ptr &args1) {
    auto &in = std::get<std::vector<double>>(Objects.at(args1));

    out.clear();
    out.reserve(in.size());
    transform(in.cbegin(), in.cend(), back_inserter(out),
              std::negate<double>());
  }

  void add(seal::Ciphertext &output, const Term::Ptr &args1,
           const Term::Ptr &args2) {
    if (!isCipher(args1)) {
      assert(isCipher(args2));
      add(output, args2, args1);
      return;
    }
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    // TODO: should a previous lowering get rid of this dispatch?
    std::visit(Overloaded{[&](const seal::Ciphertext &input2) {
                            evaluator.add(input1, input2, output);
                          },
                          [&](const seal::Plaintext &input2) {
                            evaluator.add_plain(input1, input2, output);
                          },
                          [&](const std::vector<double> &input2) {
                            throw std::runtime_error(
                                "Unsupported operation encountered");
                          }},
               Objects.at(args2));
  }

  void sub(seal::Ciphertext &output, const Term::Ptr &args1,
           const Term::Ptr &args2) {
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    std::visit(Overloaded{[&](const seal::Ciphertext &input2) {
                            evaluator.sub(input1, input2, output);
                          },
                          [&](const seal::Plaintext &input2) {
                            evaluator.sub_plain(input1, input2, output);
                          },
                          [&](const std::vector<double> &input2) {
                            throw std::runtime_error(
                                "Unsupported operation encountered");
                          }},
               Objects.at(args2));
  }

  void mul(seal::Ciphertext &output, const Term::Ptr &args1,
           const Term::Ptr &args2) {
    // swap args if arg1 is plain type and arg2 is of cipher type
    if (!isCipher(args1) && isCipher(args2)) {
      mul(output, args2, args1);
      return;
    }
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    std::visit(Overloaded{[&](const seal::Ciphertext &input2) {
                            if (args1 == args2) {
                              evaluator.square(input1, output);
                            } else {
                              evaluator.multiply(input1, input2, output);
                            }
                          },
                          [&](const seal::Plaintext &input2) {
                            evaluator.multiply_plain(input1, input2, output);
                          },
                          [&](const std::vector<double> &input2) {
                            throw std::runtime_error(
                                "Unsupported operation encountered");
                          }},
               Objects.at(args2));
  }

  void leftRotate(seal::Ciphertext &output, const Term::Ptr &args1,
                  std::int32_t rotation) {
    assert(isCipher(args1));
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    evaluator.rotate_vector(input1, rotation, galoisKeys, output);
  }

  void rightRotate(seal::Ciphertext &output, const Term::Ptr &args1,
                   std::int32_t rotation) {
    assert(isCipher(args1));
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    evaluator.rotate_vector(input1, -rotation, galoisKeys, output);
  }

  void negate(seal::Ciphertext &output, const Term::Ptr &args1) {
    assert(isCipher(args1));
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    evaluator.negate(input1, output);
  }

  void relinearize(seal::Ciphertext &output, const Term::Ptr &args1) {
    assert(isCipher(args1));
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    evaluator.relinearize(input1, relinKeys, output);
  }

  void modSwitch(seal::Ciphertext &output, const Term::Ptr &args1) {
    assert(isCipher(args1));
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    evaluator.mod_switch_to_next(input1, output);
  }

  void rescale(seal::Ciphertext &output, const Term::Ptr &args1,
               std::uint32_t divisor) {
    assert(isCipher(args1));
    seal::Ciphertext &input1 = std::get<seal::Ciphertext>(Objects.at(args1));
    evaluator.rescale_to_next(input1, output);
    output.scale() = input1.scale() / pow(2.0, divisor);
  }

  void encodeRaw(seal::Plaintext &output, const Term::Ptr &args1,
                 uint32_t scale, uint32_t level) {
    auto &in = std::get<std::vector<double>>(Objects.at(args1));

    auto ctxData = context.first_context_data();
    for (std::size_t i = 0; i < level; ++i) {
      ctxData = ctxData->next_context_data();
    }

    // If the slot count is larger than the vector size, then encode repetitions
    // of the vector to fill the slot count. This will provide the correct
    // semantics for rotations.
    assert(encoder.slot_count() % program.getVecSize() == 0);
    auto copies = encoder.slot_count() / program.getVecSize();
#ifdef EVA_USE_GALOIS
    auto &scratch = *tempVec.getLocal();
#else
    auto &scratch = tempVec;
#endif
    scratch.clear();
    scratch.reserve(encoder.slot_count());
    for (int i = 0; i < copies; ++i) {
      scratch.insert(scratch.end(), std::begin(in), std::end(in));
    }

    encoder.encode(scratch, ctxData->parms_id(), pow(2.0, scale), output);
  }

  void expandConstant(std::vector<double> &output,
                      const std::shared_ptr<ConstantValue> constant) {
    constant->expandTo(output, program.getVecSize());
  }

  template <typename T> T &initValue(const Term::Ptr &term) {
    return std::get<T>(Objects[term] = T{});
  }

public:
  SEALExecutor(Program &g, seal::SEALContext ctx, seal::CKKSEncoder &ce,
               seal::Encryptor &enc, seal::Evaluator &e, seal::GaloisKeys &gk,
               seal::RelinKeys &rk)
      : program(g), context(ctx), encoder(ce), encryptor(enc), evaluator(e),
        galoisKeys(gk), relinKeys(rk), Objects(g) {
    assert(program.getVecSize() <= encoder.slot_count());
    assert((encoder.slot_count() % program.getVecSize()) == 0);
  }

  void setInputs(const SEALValuation &inputs) {
    for (auto &in : inputs) {
      auto term = program.getInput(in.first);
      std::visit(
          Overloaded{
              [&](const seal::Ciphertext &input) { Objects[term] = input; },
              [&](const seal::Plaintext &input) { Objects[term] = input; },
              [&](const std::shared_ptr<ConstantValue> &input) {
                auto &value = initValue<std::vector<double>>(term);
                expandConstant(value, input);
              }},
          in.second);
    }
  }

  void operator()(const Term::Ptr &term) {
    if (verbosityAtLeast(Verbosity::Debug)) {
      printf("EVA: Execute t%lu = %s(", term->index,
             getOpName(term->op).c_str());
      bool first = true;
      for (auto &operand : term->getOperands()) {
        if (first) {
          first = false;
          printf("t%lu", operand->index);
        } else {
          printf(",t%lu", operand->index);
        }
      }
      printf(")\n");
      fflush(stdout);
    }

    if (term->op == Op::Input) return;
    auto args = term->getOperands();
    switch (term->op) {
    case Op::Constant: {
      auto &output = initValue<std::vector<double>>(term);
      expandConstant(output, term->get<ConstantValueAttribute>());
    } break;
    case Op::Encode: {
      assert(args.size() == 1);
      assert(isRaw(args[0]));
      auto &output = initValue<seal::Plaintext>(term);
      encodeRaw(output, args[0], term->get<EncodeAtScaleAttribute>(),
                term->get<EncodeAtLevelAttribute>());
    } break;
    case Op::Add:
      assert(args.size() == 2);
      if (isRaw(args[0]) && isRaw(args[1])) {
        auto &output = initValue<std::vector<double>>(term);
        binOpRaw<std::plus<double>>(output, args[0], args[1]);
      } else { // handles plain and cipher
        assert(isCipher(args[0]) || isPlain(args[0]));
        assert(isCipher(args[1]) || isPlain(args[1]));
        auto &output = initValue<seal::Ciphertext>(term);
        add(output, args[0], args[1]);
      }
      break;
    case Op::Sub:
      assert(args.size() == 2);
      if (isRaw(args[0]) && isRaw(args[1])) {
        auto &output = initValue<std::vector<double>>(term);
        binOpRaw<std::minus<double>>(output, args[0], args[1]);
      } else { // handles plain and cipher
        assert(isCipher(args[0]) || isPlain(args[0]));
        assert(isCipher(args[1]) || isPlain(args[1]));
        auto &output = initValue<seal::Ciphertext>(term);
        sub(output, args[0], args[1]);
      }
      break;
    case Op::Mul:
      assert(args.size() == 2);
      if (isRaw(args[0]) && isRaw(args[1])) {
        auto &output = initValue<std::vector<double>>(term);
        binOpRaw<std::multiplies<double>>(output, args[0], args[1]);
      } else { // works on cipher, no plaintext support
        assert(isCipher(args[0]) || isCipher(args[1]));
        assert(!isRaw(args[0]) && !isRaw(args[1]));
        auto &output = initValue<seal::Ciphertext>(term);
        mul(output, args[0], args[1]);
      }
      break;
    case Op::RotateLeftConst:
      assert(args.size() == 1);
      if (isRaw(args[0])) {
        auto &output = initValue<std::vector<double>>(term);
        leftRotateRaw(output, args[0], term->get<RotationAttribute>());
      } else { // works on cipher, no plaintext support
        assert(isCipher(args[0]));
        auto &output = initValue<seal::Ciphertext>(term);
        leftRotate(output, args[0], term->get<RotationAttribute>());
      }
      break;
    case Op::RotateRightConst:
      assert(args.size() == 1);
      if (isRaw(args[0])) {
        auto &output = initValue<std::vector<double>>(term);
        rightRotateRaw(output, args[0], term->get<RotationAttribute>());
      } else { // works on cipher, no plaintext support
        assert(isCipher(args[0]));
        auto &output = initValue<seal::Ciphertext>(term);
        rightRotate(output, args[0], term->get<RotationAttribute>());
      }
      break;
    case Op::Negate:
      assert(args.size() == 1);
      if (isRaw(args[0])) {
        auto &output = initValue<std::vector<double>>(term);
        negateRaw(output, args[0]);
      } else { // works on cipher, no plaintext support
        assert(isCipher(args[0]));
        auto &output = initValue<seal::Ciphertext>(term);
        negate(output, args[0]);
      }
      break;
    case Op::Relinearize: {
      assert(args.size() == 1);
      assert(isCipher(args[0]));
      auto &output = initValue<seal::Ciphertext>(term);
      relinearize(output, args[0]);
    } break;
    case Op::ModSwitch: {
      assert(args.size() == 1);
      assert(isCipher(args[0]));
      auto &output = initValue<seal::Ciphertext>(term);
      modSwitch(output, args[0]);
    } break;
    case Op::Rescale: {
      assert(args.size() == 1);
      assert(isCipher(args[0]));
      auto &output = initValue<seal::Ciphertext>(term);
      rescale(output, args[0], term->get<RescaleDivisorAttribute>());
    } break;
    case Op::Output: {
      assert(args.size() == 1);
      Objects[term] = Objects.at(args[0]);
    } break;
    default:
      throw std::runtime_error("Unhandled op " + getOpName(term->op));
    }
  }

  void free(const Term::Ptr &term) {
    if (term->op == Op::Output) {
      return;
    }
    auto &obj = Objects.at(term);
    std::visit(Overloaded{[](seal::Ciphertext &cipher) { cipher.release(); },
                          [](seal::Plaintext &plain) { plain.release(); },
                          [](std::vector<double> &raw) {
                            raw.clear();
                            raw.shrink_to_fit();
                          }},
               obj);
  }

  void getOutputs(SEALValuation &encOutputs) {
    for (auto &out : program.getOutputs()) {
      std::visit(Overloaded{[&](const seal::Ciphertext &output) {
                              encOutputs[out.first] = output;
                            },
                            [&](const seal::Plaintext &output) {
                              encOutputs[out.first] = output;
                            },
                            [&](const std::vector<double> &output) {
                              encOutputs[out.first] =
                                  std::make_shared<DenseConstantValue>(
                                      program.getVecSize(), output);
                            }},
                 Objects.at(out.second));
    }
  }
};

} // namespace eva
