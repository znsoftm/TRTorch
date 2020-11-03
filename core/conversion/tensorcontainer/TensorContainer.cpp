#include "core/conversion/tensorcontainer/TensorContainer.h"

namespace trtorch {
namespace core {
namespace conversion {
namespace {

static auto tensor_container =
    torch::class_<TensorContainer>("_trtorch_eval_ivalue_types", "TensorContainer").def(torch::init<>());
} // namespace
} // namespace conversion
} // namespace core
} // namespace trtorch