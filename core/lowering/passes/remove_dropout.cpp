#include <torch/csrc/jit/passes/subgraph_rewrite.h>

#include "core/util/prelude.h"

namespace trtorch {
namespace core {
namespace lowering {
namespace passes {

void RemoveDropout(std::shared_ptr<torch::jit::Graph>& graph) {
  std::string dropout_pattern = R"IR(
        graph(%input, %4, %5):
            %6 = aten::dropout(%input, %4, %5)
            return (%6))IR";
  std::string no_dropout_pattern = R"IR(
        graph(%input, %4, %5):
            return (%input))IR";

  torch::jit::SubgraphRewriter remove_dropout;
  remove_dropout.RegisterRewritePattern(dropout_pattern, no_dropout_pattern);
  remove_dropout.runOnGraph(graph);

  std::string dropout_inplace_pattern = R"IR(
        graph(%input, %4, %5):
            %6 = aten::dropout_(%input, %4, %5)
            return (%6))IR";
  std::string no_dropout_inplace_pattern = R"IR(
        graph(%input, %4, %5):
            return (%input))IR";

  torch::jit::SubgraphRewriter remove_dropout_inplace_pattern;
  remove_dropout_inplace_pattern.RegisterRewritePattern(dropout_inplace_pattern, no_dropout_inplace_pattern);
  remove_dropout_inplace_pattern.runOnGraph(graph);

  LOG_GRAPH("Post remove dropout: " << *graph);
}

} // namespace passes
} // namespace lowering
} // namespace core
} // namespace trtorch
