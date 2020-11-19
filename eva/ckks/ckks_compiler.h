// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "eva/ckks/always_rescaler.h"
#include "eva/ckks/ckks_config.h"
#include "eva/ckks/ckks_parameters.h"
#include "eva/ckks/ckks_signature.h"
#include "eva/ckks/eager_relinearizer.h"
#include "eva/ckks/eager_waterline_rescaler.h"
#include "eva/ckks/encode_inserter.h"
#include "eva/ckks/encryption_parameter_selector.h"
#include "eva/ckks/lazy_relinearizer.h"
#include "eva/ckks/lazy_waterline_rescaler.h"
#include "eva/ckks/levels_checker.h"
#include "eva/ckks/minimum_rescaler.h"
#include "eva/ckks/mod_switcher.h"
#include "eva/ckks/parameter_checker.h"
#include "eva/ckks/scales_checker.h"
#include "eva/ckks/seal_lowering.h"
#include "eva/common/constant_folder.h"
#include "eva/common/program_traversal.h"
#include "eva/common/reduction_balancer.h"
#include "eva/common/rotation_keys_selector.h"
#include "eva/common/type_deducer.h"
#include "eva/util/logging.h"
#include <cstdint>
#include <seal/util/hestdparms.h>

namespace eva {

class CKKSCompiler {
  CKKSConfig config;

  void transform(Program &program, TermMap<Type> &types,
                 TermMapOptional<std::uint32_t> &scales) {
    auto programRewrite = ProgramTraversal(program);

    log(Verbosity::Debug, "Running TypeDeducer pass");
    programRewrite.forwardPass(TypeDeducer(program, types));
    log(Verbosity::Debug, "Running ConstantFolder pass");
    programRewrite.forwardPass(ConstantFolder(
        program, scales)); // currently required because executor/runtime
                           // does not handle this
    if (config.balanceReductions) {
      log(Verbosity::Debug, "Running ReductionCombiner pass");
      programRewrite.forwardPass(ReductionCombiner(program));
      log(Verbosity::Debug, "Running ReductionLogExpander pass");
      programRewrite.forwardPass(ReductionLogExpander(program, types));
    }
    switch (config.rescaler) {
    case CKKSRescaler::Minimum:
      log(Verbosity::Debug, "Running MinimumRescaler pass");
      programRewrite.forwardPass(MinimumRescaler(program, types, scales));
      break;
    case CKKSRescaler::Always:
      log(Verbosity::Debug, "Running AlwaysRescaler pass");
      programRewrite.forwardPass(AlwaysRescaler(program, types, scales));
      break;
    case CKKSRescaler::EagerWaterline:
      log(Verbosity::Debug, "Running EagerWaterlineRescaler pass");
      programRewrite.forwardPass(
          EagerWaterlineRescaler(program, types, scales));
      break;
    case CKKSRescaler::LazyWaterline:
      log(Verbosity::Debug, "Running LazyWaterlineRescaler pass");
      programRewrite.forwardPass(LazyWaterlineRescaler(program, types, scales));
      break;
    default:
      throw std::logic_error("Unhandled rescaler in CKKSCompiler.");
    }
    log(Verbosity::Debug, "Running TypeDeducer pass");
    programRewrite.forwardPass(TypeDeducer(program, types));

    log(Verbosity::Debug, "Running EncodeInserter pass");
    programRewrite.forwardPass(EncodeInserter(program, types, scales));
    log(Verbosity::Debug, "Running TypeDeducer pass");
    programRewrite.forwardPass(TypeDeducer(program, types));
    // TODO: rerunning the type deducer at every step is wasteful, but also
    // forcing other passes to always keep type information up to date isn't
    // something they should need to do. Type deduction should be changed
    // into a thing that is done as needed locally.
    if (config.lazyRelinearize) {
      log(Verbosity::Debug, "Running LazyRelinearizer pass");
      programRewrite.forwardPass(LazyRelinearizer(program, types, scales));
    } else {
      log(Verbosity::Debug, "Running EagerRelinearizer pass");
      programRewrite.forwardPass(EagerRelinearizer(program, types, scales));
    }
    log(Verbosity::Debug, "Running TypeDeducer pass");
    programRewrite.forwardPass(TypeDeducer(program, types));
    log(Verbosity::Debug, "Running ModSwitcher pass");
    programRewrite.backwardPass(ModSwitcher(program, types, scales));
    log(Verbosity::Debug, "Running TypeDeducer pass");
    programRewrite.forwardPass(TypeDeducer(program, types));
    log(Verbosity::Debug, "Running SEALLowering pass");
    programRewrite.forwardPass(SEALLowering(program, types));
  }

  void validate(Program &program, TermMap<Type> &types,
                TermMapOptional<std::uint32_t> &scales) {
    auto programTraverse = ProgramTraversal(program);
    log(Verbosity::Debug, "Running LevelsChecker pass");
    LevelsChecker lc(program, types);
    programTraverse.forwardPass(lc);
    try {
      log(Verbosity::Debug, "Running ParameterChecker pass");
      ParameterChecker pc(program, types);
      programTraverse.forwardPass(pc);
    } catch (const InconsistentParameters &e) {
      switch (config.rescaler) {
      case CKKSRescaler::Minimum:
        throw std::runtime_error(
            "The 'minimum' rescaler produced inconsistent parameters. Note "
            "that this rescaling policy is not general and thus will not work "
            "for all programs. Please use a different rescaler for this "
            "program.");
      case CKKSRescaler::Always:
        throw std::runtime_error(
            "The 'always' rescaler produced inconsistent parameters. Note that "
            "this rescaling policy is not general. It is only guaranteed to "
            "work for programs that have equal scale for all inputs and "
            "constants.");
      default:
        throw std::runtime_error(
            "The current rescaler produced inconsistent parameters. This is a "
            "bug, as this rescaler should be able to handle all programs.");
      }
    }
    log(Verbosity::Debug, "Running ScalesChecker pass");
    ScalesChecker sc(program, scales, types);
    programTraverse.forwardPass(sc);
  }

  std::size_t getMinDegreeForBitCount(int (*MaxBitsFun)(std::size_t),
                                      int bitCount) {
    std::size_t degree = 1024;
    int maxBitsSeen = 0;
    while (true) {
      auto maxBitsForDegree = MaxBitsFun(degree);
      maxBitsSeen = std::max(maxBitsSeen, maxBitsForDegree);
      if (maxBitsForDegree == 0) {
        throw std::runtime_error(
            "Program requires a " + std::to_string(bitCount) +
            " bit modulus, but parameters are available for a maximum of " +
            std::to_string(maxBitsSeen));
      }
      if (maxBitsForDegree >= bitCount) {
        return degree;
      }
      degree *= 2;
    }
  }

  void determineEncryptionParameters(Program &program,
                                     CKKSParameters &encParams,
                                     TermMapOptional<std::uint32_t> &scales,
                                     TermMap<Type> types) {
    auto programTraverse = ProgramTraversal(program);
    log(Verbosity::Debug, "Running EncryptionParametersSelector pass");
    EncryptionParametersSelector eps(program, scales, types);
    programTraverse.forwardPass(eps);
    log(Verbosity::Debug, "Running RotationKeysSelector pass");
    RotationKeysSelector rks(program, types);
    programTraverse.forwardPass(rks);
    encParams.primeBits = eps.getEncryptionParameters();
    encParams.rotations = rks.getRotationKeys();

    int bitCount = 0;
    for (auto &logQ : encParams.primeBits)
      bitCount += logQ;
    if (config.securityLevel <= 128) {
      if (config.quantumSafe)
        encParams.polyModulusDegree = getMinDegreeForBitCount(
            &seal::util::seal_he_std_parms_128_tq, bitCount);
      else
        encParams.polyModulusDegree = getMinDegreeForBitCount(
            &seal::util::seal_he_std_parms_128_tc, bitCount);
    } else if (config.securityLevel <= 192) {
      if (config.quantumSafe)
        encParams.polyModulusDegree = getMinDegreeForBitCount(
            &seal::util::seal_he_std_parms_192_tq, bitCount);
      else
        encParams.polyModulusDegree = getMinDegreeForBitCount(
            &seal::util::seal_he_std_parms_192_tc, bitCount);
    } else if (config.securityLevel <= 256) {
      if (config.quantumSafe)
        encParams.polyModulusDegree = getMinDegreeForBitCount(
            &seal::util::seal_he_std_parms_256_tq, bitCount);
      else
        encParams.polyModulusDegree = getMinDegreeForBitCount(
            &seal::util::seal_he_std_parms_256_tc, bitCount);
    } else {
      throw std::runtime_error(
          "EVA has support for up to 256 bit security, but " +
          std::to_string(config.securityLevel) +
          " bit security was requested.");
    }

    auto slots = encParams.polyModulusDegree / 2;
    if (config.warnVecSize && slots > program.getVecSize()) {
      warn("Program specifies vector size %i while at least %i slots are "
           "required for security. "
           "This does not affect correctness, as the smaller vector size will "
           "be transparently emulated. "
           "However, using a vector size up to %i would come at no additional "
           "cost.",
           program.getVecSize(), slots, slots);
    }
    if (slots < program.getVecSize()) {
      if (config.warnVecSize) {
        warn("Program uses vector size %i while only %i slots are required for "
             "security. "
             "This does not affect correctness, but higher performance may be "
             "available "
             "with a smaller vector size.",
             program.getVecSize(), slots);
      }
      encParams.polyModulusDegree = 2 * program.getVecSize();
    }

    if (verbosityAtLeast(Verbosity::Info)) {
      printf("EVA: Encryption parameters for %s are:\n  Q = [",
             program.getName().c_str());
      bool first = true;
      for (auto &logQ : encParams.primeBits) {
        if (first) {
          first = false;
          printf("%i", logQ);
        } else {
          printf(",%i", logQ);
        }
      }
      int n = encParams.polyModulusDegree;
      int nexp = 0;
      while (n >>= 1)
        ++nexp;
      printf("] (total bits %i)\n  N = 2^%i (available slots %i)\n  Rotation "
             "keys: ",
             bitCount, nexp, encParams.polyModulusDegree / 2);
      first = true;
      for (auto &rotation : encParams.rotations) {
        if (first) {
          first = false;
          printf("%i", rotation);
        } else {
          printf(", %i", rotation);
        }
      }
      printf(" (count %lu)\n", encParams.rotations.size());
    }
  }

  CKKSSignature extractSignature(const Program &program) {
    std::unordered_map<std::string, CKKSEncodingInfo> inputs;
    for (auto &input : program.getInputs()) {
      Type type = input.second->get<TypeAttribute>();
      assert(type != Type::Undef);

      inputs.emplace(
          input.first,
          CKKSEncodingInfo(type, input.second->get<EncodeAtScaleAttribute>(),
                           input.second->get<EncodeAtLevelAttribute>()));
    }
    return CKKSSignature(program.getVecSize(), std::move(inputs));
  }

public:
  CKKSCompiler() {}
  CKKSCompiler(CKKSConfig config) : config(config) {}

  std::tuple<std::unique_ptr<Program>, CKKSParameters, CKKSSignature>
  compile(Program &inputProgram) {
    auto program = inputProgram.deepCopy();

    log(Verbosity::Info, "Compiling %s for CKKS with:\n%s",
        program->getName().c_str(), config.toString(2).c_str());

    TermMap<Type> types(*program);
    TermMapOptional<std::uint32_t> scales(*program);
    for (auto &source : program->getSources()) {
      // Error out if the scale attribute doesn't exist
      if (!source->has<EncodeAtScaleAttribute>()) {
        for (auto &entry : program->getInputs()) {
          if (source == entry.second) {
            throw std::runtime_error("The scale for input " + entry.first +
                                     " was not set.");
          }
        }
        throw std::runtime_error("The scale for a constant was not set.");
      }
      // Copy the scale from the attribute into the scales TermMap
      scales[source] = source->get<EncodeAtScaleAttribute>();
    }

    CKKSParameters encParams;
    transform(*program, types, scales);
    validate(*program, types, scales);
    determineEncryptionParameters(*program, encParams, scales, types);

    auto signature = extractSignature(*program);

    return std::make_tuple(std::move(program), std::move(encParams),
                           std::move(signature));
  }
};

} // namespace eva
