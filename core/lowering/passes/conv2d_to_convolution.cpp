#include <torch/csrc/jit/passes/subgraph_rewrite.h>

#include "core/util/prelude.h"

namespace trtorch {
namespace core {
namespace lowering {
namespace passes {

void Conv2DToConvolution(std::shared_ptr<torch::jit::Graph>& graph) {
  std::string conv2d_pattern = R"IR(
        graph(%x, %w, %b, %s, %p, %d, %g):
            %4 : Tensor = aten::conv2d(%x, %w, %b, %s, %p, %d, %g)
            return (%4))IR";
  std::string convolution_pattern = R"IR(
        graph(%x, %w, %b, %s, %p, %d, %g):
            %1 : bool = prim::Constant[value=0]()
            %2 : int[] = prim::Constant[value=[0, 0]]()
            %4 : Tensor = aten::_convolution(%x, %w, %b, %s, %p, %d, %1, %2, %g, %1, %1, %1)
            return (%4))IR";
  ;

  // replace matmul + add pattern to linear
  torch::jit::SubgraphRewriter map_conv2d_to_convolution;
  map_conv2d_to_convolution.RegisterRewritePattern(conv2d_pattern, convolution_pattern);
  map_conv2d_to_convolution.runOnGraph(graph);
  LOG_GRAPH("Post map conv2d -> _convolution: " << *graph);
}

} // namespace passes
} // namespace lowering
} // namespace core
} // namespace trtorch