/*******************************************************************************
 * Copyright 2017-2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "../test_utilities.h"
#include "gtest/gtest.h"
#include "ngraph_assign_clusters.h"
#include "ngraph_backend_manager.h"
#include "ngraph_mark_for_clustering.h"

#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/public/session.h"
#include "tf_graph_writer.h"

using namespace std;
namespace ng = ngraph;

namespace tensorflow {

namespace ngraph_bridge {

namespace testing {

#define ASSERT_OK(x) ASSERT_EQ((x), ::tensorflow::Status::OK());
#define ASSERT_NOT_OK(x) ASSERT_NE((x), ::tensorflow::Status::OK());

// Simple Graph
TEST(Variables, SmallGraph1) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root, varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root, var, init_value);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root, var, c);

  auto assign = ops::Assign(root, var, add);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run(
      {
          var_assign,
      },
      &ng_outputs1));
  std::cout << "initialize var: " << ng_outputs1[0].matrix<float>()
            << std::endl;
  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs2));
    // Print the output
    std::cout << "itr: " << i << " ,Result: " << ng_outputs2[0].matrix<float>()
              << std::endl;
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs3));
  std::cout << "Final var: " << ng_outputs3[0].matrix<float>() << std::endl;

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::cout << "initialize var: " << tf_outputs1[0].matrix<float>()
            << std::endl;
  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs2));
    // Print the output
    std::cout << "itr: " << i << " ,Result: " << tf_outputs2[0].matrix<float>()
              << std::endl;
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs3));
  std::cout << "Final var: " << tf_outputs3[0].matrix<float>() << std::endl;

  Compare(tf_outputs1, ng_outputs1);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
}

// Graph with 2 Assigns
TEST(Variables, SmallGraph2) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var1"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{2.f, 3.f}, {4.f, 5.f}});
  auto var_assign = ops::Assign(root, var, init_value);

  auto c = ops::Const(root, {{11.f, 12.f}, {13.f, 14.f}});

  auto add = ops::Add(root.WithOpName("Add1"), var, c);

  auto assign = ops::Assign(root, var, add);

  auto add2 = ops::Add(root.WithOpName("Add2"), assign, c);

  auto assign2 = ops::Assign(root, var, add2);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;

  ASSERT_OK(ng_session.Run({var_assign}, &ng_outputs1));
  std::cout << "initialize var: " << ng_outputs1[0].matrix<float>()
            << std::endl;

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign2}, &ng_outputs2));
    // Print the output
    std::cout << "itr: " << i << " ,Result: " << ng_outputs2[0].matrix<float>()
              << std::endl;
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs3));
  std::cout << "Final var: " << ng_outputs3[0].matrix<float>() << std::endl;

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));
  std::cout << "initialize var: " << tf_outputs1[0].matrix<float>()
            << std::endl;
  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign2}, &tf_outputs2));
    // Print the output
    std::cout << "itr: " << i << " ,Result: " << tf_outputs2[0].matrix<float>()
              << std::endl;
  }

  tf_session.Run({var}, &tf_outputs3);
  std::cout << "Final var: " << tf_outputs3[0].matrix<float>() << std::endl;

  Compare(tf_outputs1, ng_outputs1);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
}

// Graph withApplyGradientDescent which is currently not supported
TEST(Variables, SmallGraph3) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var = ops::Variable(root.WithOpName("Var1"), varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var_assign = ops::Assign(root, var, init_value);

  auto c = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto s = ops::Const(root, 1.f);
  auto d = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root, var, c);

  auto assign = ops::Assign(root, var, add);

  auto apply_gradient_descent = ops::ApplyGradientDescent(root, var, s, d);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;
  std::vector<tensorflow::Tensor> ng_outputs4;
  std::vector<tensorflow::Tensor> ng_outputs5;

  ASSERT_OK(ng_session.Run(
      {
          var_assign,
      },
      &ng_outputs1));

  std::cout << "initialize var: " << ng_outputs1[0].matrix<float>()
            << std::endl;

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs2));
    // Print the output,
    // right now prints out the TF tensor
    std::cout << "itr: " << i << " ,Result: " << ng_outputs2[0].matrix<float>()
              << std::endl;
  }
  ASSERT_OK(ng_session.Run({apply_gradient_descent}, &ng_outputs3));

  // this apply_gradient_descent result should be {{10.0,10.0},{10.0,10.0}}
  std::cout << "ApplyGradientDescent value " << ng_outputs3[0].matrix<float>()
            << std::endl;

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs4));
    // Print the output
    std::cout << "itr: " << i << " ,Result: " << ng_outputs4[0].matrix<float>()
              << std::endl;
  }

  ASSERT_OK(ng_session.Run({var}, &ng_outputs5));
  std::cout << "Final var: " << ng_outputs5[0].matrix<float>() << std::endl;

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;
  std::vector<tensorflow::Tensor> tf_outputs4;
  std::vector<tensorflow::Tensor> tf_outputs5;

  ASSERT_OK(tf_session.Run(
      {
          var_assign,
      },
      &tf_outputs1));

  std::cout << "initialize var: " << tf_outputs1[0].matrix<float>()
            << std::endl;

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs2));
    // Print the output,
    // right now prints out the TF tensor
    std::cout << "itr: " << i << " ,Result: " << tf_outputs2[0].matrix<float>()
              << std::endl;
  }
  ASSERT_OK(tf_session.Run({apply_gradient_descent}, &tf_outputs3));

  // this apply_gradient_descent result should be {{10.0,10.0},{10.0,10.0}}
  std::cout << "ApplyGradientDescent value " << tf_outputs3[0].matrix<float>()
            << std::endl;

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs4));
    // Print the output
    std::cout << "itr: " << i << " ,Result: " << tf_outputs4[0].matrix<float>()
              << std::endl;
  }

  ASSERT_OK(tf_session.Run({var}, &tf_outputs5));
  std::cout << "Final var: " << tf_outputs5[0].matrix<float>() << std::endl;

  Compare(tf_outputs1, ng_outputs1);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
  Compare(tf_outputs4, ng_outputs4);
  Compare(tf_outputs5, ng_outputs5);
}

// Graph with 2 variables
TEST(Variables, SmallGraph4) {
  Scope root = Scope::NewRootScope();

  PartialTensorShape varShape({2, 2});
  auto var1 = ops::Variable(root, varShape, DT_FLOAT);
  auto init_value = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});
  auto var1_assign = ops::Assign(root, var1, init_value);

  auto var2 = ops::Variable(root, varShape, DT_FLOAT);
  auto init_value2 = ops::Const(root, {{123.f, 34.f}, {0.f, 112121.f}});
  auto var2_assign = ops::Assign(root, var2, init_value2);

  auto s = ops::Const(root, 1.f);
  auto d = ops::Const(root, {{1.f, 1.f}, {1.f, 1.f}});

  auto add = ops::Add(root, var1, var2);
  auto assign = ops::Assign(root, var1, add);
  auto apply_gradient_descent = ops::ApplyGradientDescent(root, var2, s, d);
  auto mul = ops::Mul(root, var1, var2);
  auto assign2 = ops::Assign(root, var2, mul);
  auto mul2 = ops::Mul(root, var1, var2);

  // Turn off optimizations so that all the nodes are processed
  tensorflow::SessionOptions options;
  options.config.mutable_graph_options()
      ->mutable_optimizer_options()
      ->set_opt_level(tensorflow::OptimizerOptions_Level_L0);
  options.config.mutable_graph_options()
      ->mutable_rewrite_options()
      ->set_constant_folding(tensorflow::RewriterConfig::OFF);

  // Run on nGraph
  ActivateNGraph();
  ClientSession ng_session(root, options);
  std::vector<tensorflow::Tensor> ng_outputs1;
  std::vector<tensorflow::Tensor> ng_outputs2;
  std::vector<tensorflow::Tensor> ng_outputs3;
  std::vector<tensorflow::Tensor> ng_outputs4;
  std::vector<tensorflow::Tensor> ng_outputs5;

  ASSERT_OK(ng_session.Run({var1_assign, var2_assign}, &ng_outputs1));

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({assign}, &ng_outputs2));
    // Print the output,
    // right now prints out the TF tensor
    std::cout << "itr: " << i << " ,Result: " << ng_outputs2[0].matrix<float>()
              << std::endl;
  }

  for (int i = 0; i < 5; i++) {
    ASSERT_OK(ng_session.Run({apply_gradient_descent}, &ng_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(ng_session.Run({mul2}, &ng_outputs4));
  }

  ASSERT_OK(ng_session.Run({var1, var2}, &ng_outputs5));

  // Run on TF
  DeactivateNGraph();
  ClientSession tf_session(root, options);
  std::vector<tensorflow::Tensor> tf_outputs1;
  std::vector<tensorflow::Tensor> tf_outputs2;
  std::vector<tensorflow::Tensor> tf_outputs3;
  std::vector<tensorflow::Tensor> tf_outputs4;
  std::vector<tensorflow::Tensor> tf_outputs5;

  ASSERT_OK(tf_session.Run({var1_assign, var2_assign}, &tf_outputs1));

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({assign}, &tf_outputs2));
  }

  for (int i = 0; i < 5; i++) {
    ASSERT_OK(tf_session.Run({apply_gradient_descent}, &tf_outputs3));
  }

  for (int i = 0; i < 10; i++) {
    ASSERT_OK(tf_session.Run({mul2}, &tf_outputs4));
  }

  ASSERT_OK(tf_session.Run({var1, var2}, &tf_outputs5));

  Compare(tf_outputs1, ng_outputs1);
  Compare(tf_outputs2, ng_outputs2);
  Compare(tf_outputs3, ng_outputs3);
  Compare(tf_outputs4, ng_outputs4);
  Compare(tf_outputs5, ng_outputs5);
}

}  // namespace testing
}  // namespace ngraph_bridge
}  // namespace tensorflow