#include <string>
#include "core/compiler.h"
#include "gtest/gtest.h"
#include "tests/util/util.h"
#include "torch/csrc/jit/ir/irparser.h"

// aten::_convolution(Tensor input, Tensor weight,
//                    Tensor? bias, int[] stride, int[] padding,
//                    int[] dilation, bool transposed,
//                    int[] output_padding, int groups, bool benchmark,
//                    bool deterministic, bool cudnn_enabled) -> (Tensor)

void conv_test_helper(std::string graph_ir) {
  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph_ir, &*g);

  auto in = at::randint(1, 10, {1, 3, 10, 10}, {at::kCUDA});
  auto w = at::randint(1, 10, {8, 3, 5, 5}, {at::kCUDA});
  auto b = at::randint(1, 10, {8}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolutionConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(8:45, 3:15, 5:5, 5:1),
            %2 : Float(8:1)):
        %3 : int = prim::Constant[value=1]()
        %4 : int = prim::Constant[value=0]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=0]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %3, %7, %7, %7)
        return (%12))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 3, 10, 10}, {at::kCUDA});
  auto w = at::randint(1, 10, {8, 3, 5, 5}, {at::kCUDA});
  auto b = at::randint(1, 10, {8}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolutionNoBiasConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(4:9, 1:9, 3:3, 3:1)):
        %2 : None = prim::Constant()
        %3 : int = prim::Constant[value=1]()
        %4 : int = prim::Constant[value=0]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=0]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %3, %7, %7, %7)
        return (%12))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 2, {1, 1, 3, 3}, {at::kCUDA});
  auto w = at::randint(1, 2, {4, 1, 2, 2}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolutionWithStrideConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(4:27, 3:9, 3:3, 3:1),
            %2 : Float(4:1)):
        %3 : int = prim::Constant[value=3]()
        %4 : int = prim::Constant[value=0]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=0]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : int = prim::Constant[value=1]()
        %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %12, %7, %7, %7)
        return (%13))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 3, 9, 9}, {at::kCUDA});
  auto w = at::randint(1, 10, {4, 3, 3, 3}, {at::kCUDA});
  auto b = at::randint(1, 10, {4}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolutionWithPaddingConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(4:48, 3:16, 4:4, 4:1),
            %2 : Float(4:1)):
        %3 : int = prim::Constant[value=1]()
        %4 : int = prim::Constant[value=2]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=0]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : int = prim::Constant[value=1]()
        %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %12, %7, %7, %7)
        return (%13))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 3, 4, 4}, {at::kCUDA});
  auto w = at::randint(1, 10, {4, 3, 2, 2}, {at::kCUDA});
  auto b = at::randint(1, 10, {4}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolution3dConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(32:81, 3:27, 3:9, 3:3, 3:1),
            %2 : Float(32:1)):
        %sv : int = prim::Constant[value=1]()
        %s : int[] = prim::ListConstruct(%sv, %sv, %sv)
        %pv : int = prim::Constant[value=0]()
        %p : int[] = prim::ListConstruct(%pv, %pv, %pv)
        %transposed : bool = prim::Constant[value=0]()
        %opv : int = prim::Constant[value=0]()
        %op : int[] = prim::ListConstruct(%opv, %opv, %opv)
        %g : int = prim::Constant[value=1]()
        %fb : bool = prim::Constant[value=0]()
        %out : Tensor = aten::_convolution(%0, %1, %2, %s, %p, %s, %transposed, %op, %g, %fb, %fb, %fb)
        return (%out))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 3, 5, 5, 5}, {at::kCUDA});
  auto w = at::randint(1, 10, {32, 3, 3, 3, 3}, {at::kCUDA});
  auto b = at::randint(1, 10, {32}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolution3dNoBiasConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(32:81, 3:27, 3:9, 3:3, 3:1)):
        %bias : None = prim::Constant()
        %sv : int = prim::Constant[value=1]()
        %s : int[] = prim::ListConstruct(%sv, %sv, %sv)
        %pv : int = prim::Constant[value=0]()
        %p : int[] = prim::ListConstruct(%pv, %pv, %pv)
        %transposed : bool = prim::Constant[value=0]()
        %opv : int = prim::Constant[value=0]()
        %op : int[] = prim::ListConstruct(%opv, %opv, %opv)
        %g : int = prim::Constant[value=1]()
        %fb : bool = prim::Constant[value=0]()
        %out : Tensor = aten::_convolution(%0, %1, %bias, %s, %p, %s, %transposed, %op, %g, %fb, %fb, %fb)
        return (%out))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 2, {1, 3, 5, 5, 5}, {at::kCUDA});
  auto w = at::randint(1, 2, {32, 3, 3, 3, 3}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolution3dWithPaddingConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(32:81, 3:27, 3:9, 3:3, 3:1),
            %2 : Float(32:1)):
        %sv : int = prim::Constant[value=1]()
        %s : int[] = prim::ListConstruct(%sv, %sv, %sv)
        %pv : int = prim::Constant[value=1]()
        %p : int[] = prim::ListConstruct(%pv, %pv, %pv)
        %transposed : bool = prim::Constant[value=0]()
        %opv : int = prim::Constant[value=0]()
        %op : int[] = prim::ListConstruct(%opv, %opv, %opv)
        %g : int = prim::Constant[value=1]()
        %fb : bool = prim::Constant[value=0]()
        %out : Tensor = aten::_convolution(%0, %1, %2, %s, %p, %s, %transposed, %op, %g, %fb, %fb, %fb)
        return (%out))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 3, 5, 5, 5}, {at::kCUDA});
  auto w = at::randint(1, 10, {32, 3, 3, 3, 3}, {at::kCUDA});
  auto b = at::randint(1, 10, {32}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvolution3dWithStrideDilationConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(32:81, 3:27, 3:9, 3:3, 3:1),
            %2 : Float(32:1)):
        %sv : int = prim::Constant[value=2]()
        %s : int[] = prim::ListConstruct(%sv, %sv, %sv)
        %pv : int = prim::Constant[value=1]()
        %p : int[] = prim::ListConstruct(%pv, %pv, %pv)
        %transposed : bool = prim::Constant[value=0]()
        %opv : int = prim::Constant[value=0]()
        %op : int[] = prim::ListConstruct(%opv, %opv, %opv)
        %g : int = prim::Constant[value=1]()
        %fb : bool = prim::Constant[value=0]()
        %out : Tensor = aten::_convolution(%0, %1, %2, %s, %p, %s, %transposed, %op, %g, %fb, %fb, %fb)
        return (%out))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 3, 5, 5, 5}, {at::kCUDA});
  auto w = at::randint(1, 10, {32, 3, 3, 3, 3}, {at::kCUDA});
  auto b = at::randint(1, 10, {32}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvTransposeConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(8:27, 3:9, 3:3, 3:1),
            %2 : Float(8:1)):
        %3 : int = prim::Constant[value=1]()
        %4 : int = prim::Constant[value=0]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=1]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %3, %7, %7, %7)
        return (%12))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 3, {1, 8, 5, 5}, {at::kCUDA});
  auto w = at::randint(1, 3, {8, 3, 3, 3}, {at::kCUDA});
  auto b = at::randint(1, 3, {3}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvTransposeNoBiasConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(4:9, 1:9, 3:3, 3:1)):
        %2 : None = prim::Constant()
        %3 : int = prim::Constant[value=1]()
        %4 : int = prim::Constant[value=0]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=1]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %3, %7, %7, %7)
        return (%12))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 2, {1, 4, 3, 3}, {at::kCUDA});
  auto w = at::randint(1, 2, {4, 1, 2, 2}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvTransposeWithStrideConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(4:27, 3:9, 3:3, 3:1),
            %2 : Float(4:1)):
        %3 : int = prim::Constant[value=3]()
        %4 : int = prim::Constant[value=0]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=1]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : int = prim::Constant[value=1]()
        %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %12, %7, %7, %7)
        return (%13))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 4, 9, 9}, {at::kCUDA});
  auto w = at::randint(1, 10, {4, 3, 3, 3}, {at::kCUDA});
  auto b = at::randint(1, 10, {3}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

TEST(Converters, ATenConvTransposeWithPaddingConvertsCorrectly) {
  const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(4:48, 3:16, 4:4, 4:1),
            %2 : Float(4:1)):
        %3 : int = prim::Constant[value=1]()
        %4 : int = prim::Constant[value=2]()
        %5 : int = prim::Constant[value=1]()
        %6 : int = prim::Constant[value=0]()
        %7 : bool = prim::Constant[value=1]()
        %8 : int[] = prim::ListConstruct(%3, %3)
        %9 : int[] = prim::ListConstruct(%4, %4)
        %10 : int[] = prim::ListConstruct(%5, %5)
        %11 : int[] = prim::ListConstruct(%6, %6)
        %12 : int = prim::Constant[value=1]()
        %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11, %12, %7, %7, %7)
        return (%13))IR";

  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(graph, &*g);

  auto in = at::randint(1, 10, {1, 4, 4, 4}, {at::kCUDA});
  auto w = at::randint(1, 10, {4, 3, 2, 2}, {at::kCUDA});
  auto b = at::randint(1, 10, {3}, {at::kCUDA});

  auto jit_in = at::clone(in);
  auto jit_w = at::clone(w);
  auto jit_b = at::clone(b);

  auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
  auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

  auto trt_in = at::clone(in);
  auto trt_w = at::clone(w);
  auto trt_b = at::clone(b);
  params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
  auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});

  auto trt = trt_results[0].reshape(jit_results[0].sizes());

  ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt, 2e-6));
}

// TEST(Converters, ATenConvolutionWithDialationConvertsCorrectly) {
//    const auto graph = R"IR(
//       graph(%0 : Tensor,
//             %1 : Float(8, 3, 5, 5),
//             %2 : Float(8)):
//         %3 : int = prim::Constant[value=1]()
//         %4 : int = prim::Constant[value=0]()
//         %5 : int = prim::Constant[value=2]()
//         %6 : int = prim::Constant[value=0]()
//         %7 : bool = prim::Constant[value=0]()
//         %8 : int[] = prim::ListConstruct(%3, %3)
//         %9 : int[] = prim::ListConstruct(%4, %4)
//         %10 : int[] = prim::ListConstruct(%5, %5)
//         %11 : int[] = prim::ListConstruct(%6, %6)
//         %12 : int = prim::Constant[value=1]()
//         %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11,
//         %12, %7, %7, %7) return (%13))IR";

//     conv_test_helper(graph);
// }

// TEST(Converters, ATenConvolutionWithPostPaddingConvertsCorrectly) {
//    const auto graph = R"IR(
//       graph(%0 : Tensor,
//             %1 : Float(8, 3, 5, 5),
//             %2 : Float(8)):
//         %3 : int = prim::Constant[value=1]()
//         %4 : int = prim::Constant[value=0]()
//         %5 : int = prim::Constant[value=1]()
//         %6 : int = prim::Constant[value=2]()
//         %7 : bool = prim::Constant[value=0]()
//         %8 : int[] = prim::ListConstruct(%3, %3)
//         %9 : int[] = prim::ListConstruct(%4, %4)
//         %10 : int[] = prim::ListConstruct(%5, %5)
//         %11 : int[] = prim::ListConstruct(%6, %6)
//         %12 : int = prim::Constant[value=1]()
//         %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11,
//         %12, %7, %7, %7) return (%13))IR";

//     conv_test_helper(graph);
// }

// TEST(Converters, ATenConvolutionWithGroupConvertsCorrectly) {
//    const auto graph = R"IR(
//       graph(%0 : Tensor,
//             %1 : Float(8, 3, 5, 5),
//             %2 : Float(8)):
//         %3 : int = prim::Constant[value=1]()
//         %4 : int = prim::Constant[value=0]()
//         %5 : int = prim::Constant[value=1]()
//         %6 : int = prim::Constant[value=0]()
//         %7 : bool = prim::Constant[value=0]()
//         %8 : int[] = prim::ListConstruct(%3, %3)
//         %9 : int[] = prim::ListConstruct(%4, %4)
//         %10 : int[] = prim::ListConstruct(%5, %5)
//         %11 : int[] = prim::ListConstruct(%6, %6)
//         %12 : int = prim::Constant[value=2]()
//         %13 : Tensor = aten::_convolution(%0, %1, %2, %8, %9, %10, %7, %11,
//         %12, %7, %7, %7) return (%13))IR";

//     conv_test_helper(graph);
// }
