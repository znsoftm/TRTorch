#include <limits>

#include "torch/csrc/jit/ir/ir.h"
//#include "torch/csrc/jit/ir/constants.h"
#include "ATen/core/List.h"
#include "ATen/core/functional.h"
#include "ATen/core/ivalue.h"
#include "ATen/core/stack.h"
#include "c10/util/intrusive_ptr.h"
#include "torch/torch.h"

#include "core/conversion/evaluators/eval_macros.h"
#include "core/conversion/evaluators/eval_util.h"
#include "core/conversion/evaluators/evaluators.h"

namespace trtorch {
namespace core {
namespace conversion {
namespace evaluators {
namespace {

auto prim_registrations =
    RegisterNodeEvaluators()
        .evaluator({torch::jit::prim::Constant,
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      if (n->output()->type()->kind() == at::FunctionType::Kind) {
                        return {};
                      }
                      return evaluators::toIValue(n->output());
                    }})
        .evaluator({torch::jit::prim::NumToTensor,
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      return at::scalar_to_tensor(args.at(n->output(0)).IValue()->toScalar());
                    }})
        .evaluator({torch::jit::prim::ListConstruct,
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      const auto num_inputs = n->inputs().size();
                      if (constTypesOnly(args)) {
                        c10::ListTypePtr lt = n->output()->type()->expect<c10::ListType>();
                        if (torch::jit::IntType::get() == lt->getElementType()) {
                          c10::List<int64_t> list;
                          list.reserve(num_inputs);
                          for (auto in : n->inputs()) {
                            list.emplace_back(std::move(args.at(in).unwrapToInt()));
                          }
                          return c10::optional<torch::jit::IValue>(std::move(torch::jit::IValue(list)));
                        } else if (torch::jit::FloatType::get() == lt->getElementType()) {
                          c10::List<double> list;
                          list.reserve(num_inputs);
                          for (auto in : n->inputs()) {
                            list.emplace_back(std::move(args.at(in).unwrapToDouble()));
                          }
                          return c10::optional<torch::jit::IValue>(std::move(torch::jit::IValue(list)));
                        } else if (lt->getElementType() == torch::jit::BoolType::get()) {
                          c10::List<bool> list;
                          list.reserve(num_inputs);
                          for (auto in : n->inputs()) {
                            list.emplace_back(std::move(args.at(in).unwrapToBool()));
                          }
                          return c10::optional<torch::jit::IValue>(std::move(torch::jit::IValue(list)));
                        } else if (lt->getElementType()->isSubtypeOf(torch::jit::TensorType::get())) {
                          c10::List<at::Tensor> list;
                          list.reserve(num_inputs);
                          for (auto in : n->inputs()) {
                            if (args.at(in).isIValue()) {
                              list.emplace_back(std::move(args.at(in).unwrapToTensor()));
                            }
                          }
                          return c10::optional<torch::jit::IValue>(std::move(torch::jit::IValue(list)));
                        } else {
                          c10::TypePtr elementType = lt->getElementType();
                          auto list = c10::impl::GenericList(elementType);
                          list.reserve(num_inputs);
                          for (auto in : n->inputs()) {
                            list.emplace_back(std::move(*(args.at(in).IValue())));
                          }
                          return c10::optional<torch::jit::IValue>(std::move(torch::jit::IValue(list)));
                        }
                      } else {
                        c10::ListTypePtr lt = n->output()->type()->expect<c10::ListType>();
                        c10::TypePtr elementType = lt->getElementType();
                        auto list = c10::impl::GenericList(elementType);
                        list.reserve(num_inputs);
                        for (auto in : n->inputs()) {
                          if (args.at(in).isITensor()) {
                            auto tensor_holder = TensorContainer();
                            tensor_holder.hold_tensor(args.at(in).ITensor());
                            auto ival = c10::IValue(std::move(c10::make_intrusive<TensorContainer>(tensor_holder)));
                            list.emplace_back(std::move(ival));
                          } else {
                            list.emplace_back(std::move(args.at(in).unwrapToTensor()));
                          }
                        }
                        return c10::optional<torch::jit::IValue>(std::move(torch::jit::IValue(list)));
                      }
                    }})
        .evaluator({c10::Symbol::fromQualString("prim::min"),
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      if (n->inputs().size() == 1) {
                        auto a = args.at(n->input(0)).unwrapToIntList();
                        int64_t min = std::numeric_limits<int64_t>::max();

                        for (size_t i = 0; i < a.size(); i++) {
                          if (a[i] < min) {
                            min = a[i];
                          }
                        }

                        return min;
                      } else if (n->inputs().size() == 2) {
                        if (args.at(n->input(0)).IValue()->isInt()) {
                          auto a = args.at(n->input(0)).unwrapToInt();
                          if (args.at(n->input(1)).IValue()->isInt()) {
                            auto b = args.at(n->input(1)).unwrapToInt();
                            return a < b ? a : b;
                          } else if (args.at(n->input(1)).IValue()->isDouble()) {
                            auto b = args.at(n->input(1)).unwrapToDouble();
                            return a < b ? a : b;
                          } else {
                            TRTORCH_THROW_ERROR(
                                "Unimplemented data type for " << n->kind().toQualString() << " evaluator b arg: "
                                                               << args.at(n->input(1)).IValue()->type()->str());
                            return {};
                          }
                        } else if (args.at(n->input(0)).IValue()->isDouble()) {
                          auto a = args.at(n->input(0)).unwrapToDouble();
                          if (args.at(n->input(1)).IValue()->isInt()) {
                            auto b = args.at(n->input(1)).unwrapToInt();
                            return a < b ? a : b;
                          } else if (args.at(n->input(1)).IValue()->isDouble()) {
                            auto b = args.at(n->input(1)).unwrapToDouble();
                            return a < b ? a : b;
                          } else {
                            TRTORCH_THROW_ERROR(
                                "Unimplemented data type for " << n->kind().toQualString() << " evaluator b arg: "
                                                               << args.at(n->input(1)).IValue()->type()->str());
                            return {};
                          }
                        } else {
                          TRTORCH_THROW_ERROR(
                              "Unimplemented data type for " << n->kind().toQualString() << " evaluator a arg: "
                                                             << args.at(n->input(0)).IValue()->type()->str());
                          return {};
                        }
                      } else {
                        TRTORCH_THROW_ERROR("Unimplemented " << n->kind().toQualString() << " evaluator case");
                        return {};
                      }
                    },
                    EvalOptions().validSchemas({
                        "prim::min.self_int(int[] self) -> (int)",
                        "prim::min.bool(bool a, bool b) -> (bool)",
                        "prim::min.int(int a, int b) -> (bool)",
                        "prim::min.float(float a, float b) -> (bool)",
                        "prim::min.int_float(int a, float b) -> (bool)",
                        "prim::min.float_int(float a, int b) -> (bool)",
                    })})
        .evaluator({c10::Symbol::fromQualString("prim::max"),
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      if (n->inputs().size() == 1) {
                        auto a = args.at(n->input(0)).unwrapToIntList();
                        int64_t max = std::numeric_limits<int64_t>::min();

                        for (size_t i = 0; i < a.size(); i++) {
                          if (a[i] > max) {
                            max = a[i];
                          }
                        }

                        return max;
                      } else if (n->inputs().size() == 2) {
                        if (args.at(n->input(0)).IValue()->isInt()) {
                          auto a = args.at(n->input(0)).unwrapToInt();
                          if (args.at(n->input(1)).IValue()->isInt()) {
                            auto b = args.at(n->input(1)).unwrapToInt();
                            return a > b ? a : b;
                          } else if (args.at(n->input(1)).IValue()->isDouble()) {
                            auto b = args.at(n->input(1)).unwrapToDouble();
                            return a > b ? a : b;
                          } else {
                            TRTORCH_THROW_ERROR(
                                "Unimplemented data type for " << n->kind().toQualString() << " evaluator b arg: "
                                                               << args.at(n->input(1)).IValue()->type()->str());
                            return {};
                          }
                        } else if (args.at(n->input(0)).IValue()->isDouble()) {
                          auto a = args.at(n->input(0)).unwrapToDouble();
                          if (args.at(n->input(1)).IValue()->isInt()) {
                            auto b = args.at(n->input(1)).unwrapToInt();
                            return a > b ? a : b;
                          } else if (args.at(n->input(1)).IValue()->isDouble()) {
                            auto b = args.at(n->input(1)).unwrapToDouble();
                            return a > b ? a : b;
                          } else {
                            TRTORCH_THROW_ERROR(
                                "Unimplemented data type for " << n->kind().toQualString() << " evaluator b arg: "
                                                               << args.at(n->input(1)).IValue()->type()->str());
                            return {};
                          }
                        } else {
                          TRTORCH_THROW_ERROR(
                              "Unimplemented data type for " << n->kind().toQualString() << " evaluator a arg: "
                                                             << args.at(n->input(0)).IValue()->type()->str());
                          return {};
                        }
                      } else {
                        TRTORCH_THROW_ERROR("Unimplemented " << n->kind().toQualString() << " evaluator case");
                        return {};
                      }
                    },
                    EvalOptions().validSchemas({
                        "prim::max.self_int(int[] self) -> (int)",
                        "prim::max.bool(bool a, bool b) -> (bool)",
                        "prim::max.int(int a, int b) -> (bool)",
                        "prim::max.float(float a, float b) -> (bool)",
                        "prim::max.int_float(int a, float b) -> (bool)",
                        "prim::max.float_int(float a, int b) -> (bool)",
                    })})
        .evaluator({c10::Symbol::fromQualString("prim::shape"),
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      LOG_WARNING("There may be undefined behavior using dynamic shape and prim::shape");
                      auto tensor_var = args.at(n->input(0));
                      if (tensor_var.isITensor()) {
                        auto tensor = tensor_var.ITensor();
                        return util::toVec(tensor->getDimensions());
                      } else {
                        auto tensor = tensor_var.unwrapToTensor();
                        return tensor.sizes();
                      }
                    },
                    EvalOptions().validSchemas({"prim::shape(Tensor a) -> (int[])"})})
        .evaluator({c10::Symbol::fromQualString("prim::unchecked_cast"),
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      return *(args.at(n->input(0)).IValue());
                    }})
        .evaluator({c10::Symbol::fromQualString("prim::Uninitialized"),
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      return c10::IValue::uninitialized();
                    }})
        .evaluator({c10::Symbol::fromQualString("prim::RaiseException"),
                    [](const torch::jit::Node* n, kwargs& args) -> c10::optional<torch::jit::IValue> {
                      auto exception = args.at(n->input(0)).IValue();
                      TRTORCH_THROW_ERROR("Error from TorchScript: " << *exception);
                      return {};
                    }});
}
} // namespace evaluators
} // namespace conversion
} // namespace core
} // namespace trtorch
