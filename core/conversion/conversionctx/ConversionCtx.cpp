#include <iostream>
#include <sstream>
#include <utility>

#include "core/conversion/conversionctx/ConversionCtx.h"

namespace trtorch {
namespace core {
namespace conversion {

// clang-format off
std::ostream& operator<<(std::ostream& os, const BuilderSettings& s) {
  os << "Settings requested for TensorRT engine:"
     << "\n    Operating Precision: " << s.op_precision
     << "\n    Make Refittable Engine: " << s.refit
     << "\n    Debuggable Engine: " << s.debug
     << "\n    Strict Types: " << s.strict_types
     << "\n    Allow GPU Fallback (if running on DLA): " << s.allow_gpu_fallback
     << "\n    Min Timing Iterations: " << s.num_min_timing_iters
     << "\n    Avg Timing Iterations: " << s.num_avg_timing_iters
     << "\n    Max Workspace Size: " << s.workspace_size;

  if (s.max_batch_size != 0) {
    os << "\n    Max Batch Size: " << s.max_batch_size;
  } else {
    os << "\n    Max Batch Size: Not set";
  }

  os << "\n    Device Type: " << s.device << "\n    Engine Capability: " << s.capability
     << "\n    Calibrator Created: " << (s.calibrator != nullptr);
  return os;
}
// clang-format on

ConversionCtx::ConversionCtx(BuilderSettings build_settings)
    : settings(build_settings),
      logger(
          "[TRTorch Conversion Context] - ",
          util::logging::get_logger().get_reportable_severity(),
          util::logging::get_logger().get_is_colored_output_on()) {
  // TODO: Support FP16 and FP32 from JIT information
  builder = nvinfer1::createInferBuilder(logger);
  net = builder->createNetworkV2(1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH));

  LOG_DEBUG(build_settings);
  cfg = builder->createBuilderConfig();

  switch (settings.op_precision) {
    case nvinfer1::DataType::kHALF:
      TRTORCH_CHECK(builder->platformHasFastFp16(), "Requested inference in FP16 but platform does support FP16");
      cfg->setFlag(nvinfer1::BuilderFlag::kFP16);
      input_type = nvinfer1::DataType::kHALF;
      break;
    case nvinfer1::DataType::kINT8:
      TRTORCH_CHECK(builder->platformHasFastInt8(), "Requested inference in INT8 but platform does support INT8");
      cfg->setFlag(nvinfer1::BuilderFlag::kINT8);
      if (!settings.strict_types) {
        cfg->setFlag(nvinfer1::BuilderFlag::kFP16);
      }
      input_type = nvinfer1::DataType::kFLOAT;
      TRTORCH_CHECK(
          settings.calibrator != nullptr,
          "Requested inference in INT8 but no calibrator provided, set the ptq_calibrator field in the CompileSpec struct with your calibrator");
      cfg->setInt8Calibrator(settings.calibrator);
      break;
    case nvinfer1::DataType::kFLOAT:
    default:
      input_type = nvinfer1::DataType::kFLOAT;
      break;
  }
  op_precision = settings.op_precision;

  if (settings.refit) {
    cfg->setFlag(nvinfer1::BuilderFlag::kREFIT);
  }

  if (settings.debug) {
    cfg->setFlag(nvinfer1::BuilderFlag::kDEBUG);
  }

  if (settings.strict_types) {
    cfg->setFlag(nvinfer1::BuilderFlag::kSTRICT_TYPES);
  }

  if (settings.allow_gpu_fallback) {
    cfg->setFlag(nvinfer1::BuilderFlag::kGPU_FALLBACK);
  }

  if (settings.max_batch_size != 0) {
    builder->setMaxBatchSize(settings.max_batch_size);
  }

  cfg->setMinTimingIterations(settings.num_min_timing_iters);
  cfg->setAvgTimingIterations(settings.num_avg_timing_iters);
  cfg->setMaxWorkspaceSize(settings.workspace_size);
  cfg->setDefaultDeviceType(settings.device);
  cfg->setEngineCapability(settings.capability);
}

ConversionCtx::~ConversionCtx() {
  builder->destroy();
  net->destroy();
  cfg->destroy();
  for (auto ptr : builder_resources) {
    free(ptr);
  }
}

nvinfer1::ITensor* ConversionCtx::AssociateValueAndTensor(const torch::jit::Value* value, nvinfer1::ITensor* tensor) {
  tensor->setName(value->debugName().c_str());
  this->value_tensor_map[value] = tensor;
  return tensor;
}

torch::jit::IValue* ConversionCtx::AssociateValueAndIValue(const torch::jit::Value* value, torch::jit::IValue ivalue) {
  this->evaluated_value_map[value] = std::move(ivalue);
  return &this->evaluated_value_map[value];
}

std::string ConversionCtx::SerializeEngine() {
  auto engine = builder->buildEngineWithConfig(*net, *cfg);
  auto serialized_engine = engine->serialize();
  engine->destroy();
  return std::string((const char*)serialized_engine->data(), serialized_engine->size());
}

bool ConversionCtx::CheckLayerAddition(const torch::jit::Node* n) {
  for (auto out : n->outputs()) {
    auto iter_t = this->value_tensor_map.find(out);
    if (iter_t == this->value_tensor_map.end()) {
      auto iter_iv = this->evaluated_value_map.find(out);
      if (iter_iv == this->evaluated_value_map.end()) {
        LOG_WARNING(
            "Node "
            << util::node_info(n) << " output: " << out->debugName()
            << " does not have a coresponding value or tensor, may potentially indicate a defective evaluator or converter");
        return false;
      }
    }
  }
  return true;
}

} // namespace conversion
} // namespace core
} // namespace trtorch
