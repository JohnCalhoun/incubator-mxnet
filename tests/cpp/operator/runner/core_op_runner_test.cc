/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 *  \file core_op_runner.cc
 *  \brief Test operator runner (unary and binary ops validated here)
 *  \note This is NOT where you test your operator performance. These tests validate that
 *        the testing framework is functional.
 *  \author Chris Olivier
 */

#include <gtest/gtest.h>
#include <mxnet/tensor_blob.h>
#include <mxnet/imperative.h>
#include "../../src/imperative/imperative_utils.h"
#include "../include/test_op_runner.h"
#include "../include/test_core_op.h"

using namespace mxnet;

using kwargs_t = test::op::kwargs_t;

static const kwargs_t basic_args = {};

static const std::vector<std::pair<std::string, std::string>> test_unary_operators = {
  { "relu",    "" },  // Code can figure out what the backward op is for some
  { "sigmoid", "" },
  { "sqrt",    "" }
};

static const std::vector<std::pair<std::string, std::string>> test_binary_operators = {
  { "elemwise_add", "_backward_add" },
  { "elemwise_mul", "_backward_mul" }
};

template<typename TT>
inline std::vector<TT> AsVect(const TT& t) {
  return std::move(std::vector<TT>({ t }));
}

/*!
 * \brief Scan operators and check for registered FGradient
 */
TEST(CORE_OP_RUNNER, ScanOperatorsForFGradients) {
    std::vector<std::string> names = dmlc::Registry<Op>::ListAllNames();
    std::cout << "total ops: " << names.size() << "\n";
    std::vector<std::string> opsWithOutGradient;
    std::stringstream ss; 
    ss << "The Following ops do not have an FGradient registered: ";

    for (std::vector<std::string>::iterator i = names.begin(); i != names.end(); ++i) {
        const Op* op = dmlc::Registry<Op>::Find(*i);
        auto gradient = op->GetAttr<nnvm::FGradient>("FGradient");
        if (gradient.count(op) == 0) {
            ss << op->name << ", ";
            opsWithOutGradient.push_back(op->name);
        }
    }
    ASSERT_TRUE(opsWithOutGradient.size() == 0) << ss.str();
}

/*!
 * \brief Generic bidirectional sanity test for simple unary op
 */
TEST(CORE_OP_RUNNER, ExecuteBidirectionalSimpleUnaryList) {
  TShape shape({5, 5});
  kwargs_t kwargs = basic_args;

  for (const std::pair<std::string, std::string>& i : test_unary_operators) {
    const char *op_name = i.first.c_str();
    const char *backward_op_name = i.second.c_str();

    test::op::CoreOpExecutor<float> op(false, AsVect(shape));
    op.set_verbose(false);

    op.Init(op.ArgsWithOpName(kwargs, op_name, backward_op_name));

    PRINT_NDARRAYS(op.ctx().run_ctx, op.inputs());
    PRINT_NDARRAYS(op.ctx().run_ctx, op.outputs());
    op.Execute();
    PRINT_NDARRAYS(op.ctx().run_ctx, op.outputs());

    PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_inputs());
    PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_outputs());
    op.ExecuteBackward();
    PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_outputs());
  }
}

/*!
 * \brief Generic bidirectional sanity test for binary op
 */
TEST(CORE_OP_RUNNER, ExecuteBidirectionalList) {
  for (const std::pair<std::string, std::string>& i : test_binary_operators) {
    const char *op_name = i.first.c_str();
    const char *backward_op_name = i.second.c_str();

    TShape shape({5, 5});
    kwargs_t kwargs = basic_args;

    test::op::CoreOpExecutor<float> op(false, AsVect(shape));

    op.set_verbose(false);
    op.Init(op.ArgsWithOpName(kwargs, op_name, backward_op_name));

    PRINT_NDARRAYS(op.ctx().run_ctx, op.inputs());
    PRINT_NDARRAYS(op.ctx().run_ctx, op.outputs());
    op.Execute();
    PRINT_NDARRAYS(op.ctx().run_ctx, op.outputs());

    PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_inputs());
    PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_outputs());
    op.ExecuteBackward();
    PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_outputs());
  }
}

/*!
 * \brief Execute bidirectional dot product, which has different shaped inputs and outputs
 */
TEST(CORE_OP_RUNNER, ExecuteBidirectionalDotProduct) {
  const char *op_name = "dot";
  const char *backward_op_name = "_backward_dot";

  kwargs_t kwargs = basic_args;

  test::op::CoreOpExecutor<float> op(false, { TShape({ 2, 3 }), TShape({ 3, 2 }) });

  op.set_verbose(false);
  op.Init(op.ArgsWithOpName(kwargs, op_name, backward_op_name));

  PRINT_NDARRAYS(op.ctx().run_ctx, op.inputs());
  PRINT_NDARRAYS(op.ctx().run_ctx, op.outputs());
  op.Execute();
  PRINT_NDARRAYS(op.ctx().run_ctx, op.outputs());

  PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_inputs());
  PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_outputs());
  op.ExecuteBackward();
  PRINT_NDARRAYS(op.ctx().run_ctx, op.bwd_outputs());
}

TEST(CORE_OP_RUNNER, ExecuteBidirectionalRunnerSimpleUnary) {
  typedef float DType;
  TShape shape({5, 5});
  for (const std::pair<std::string, std::string>& i : test_unary_operators) {
    const char *op_name = i.first.c_str();
    const char *backward_op_name = i.second.c_str();
    test::op::CoreOperatorRunner<DType> runner;
    runner.RunBidirectional(false, { shape }, test::op::CoreOpExecutor<DType>::ArgsWithOpName(
      basic_args, op_name, backward_op_name), 1);
  }
}

TEST(CORE_OP_RUNNER, ExecuteBidirectionalRunner) {
  typedef float DType;
  TShape shape({5, 5});
  for (const std::pair<std::string, std::string>& i : test_binary_operators) {
    const char *op_name = i.first.c_str();
    const char *backward_op_name = i.second.c_str();
    test::op::CoreOperatorRunner<DType> runner;
    runner.RunBidirectional(false, { shape }, test::op::CoreOpExecutor<DType>::ArgsWithOpName(
      basic_args, op_name, backward_op_name), 1);
  }
}

/*!
 * \brief Test RunBidirectional dot product, which has different shaped inputs and outputs
 */
TEST(CORE_OP_RUNNER, ExecuteBidirectionalRunnerDotProduct) {
  typedef float DType;
  const char *op_name = "dot";
  const char *backward_op_name = "_backward_dot";
  test::op::CoreOperatorRunner<DType> runner;
  runner.RunBidirectional(false,
                          { TShape({ 2, 3 }), TShape({ 3, 2 }) },
                          test::op::CoreOpExecutor<DType>::ArgsWithOpName(basic_args,
                                                                          op_name,
                                                                          backward_op_name),
                          1);
}

/*!
 * \brief Timing tests for CPU
 */
TEST(CORE_OP_RUNNER, TimingCPUSimpleUnary) {
  typedef float DType;

  const char *op_name = "relu";

  const kwargs_t kwargs = test::op::CoreOpExecutor<DType>::ArgsWithOpName(basic_args, op_name);

  test::op::CoreOperatorRunner<DType> runner;
  runner.RunBidirectional(false, { TShape({10, 10, 10, 10}) }, kwargs, 1);  // prime code and cache

  std::vector <TShape> shapes;
  if (test::performance_run) {
    shapes = {
      {1,  1, 28,  28},
      {1,  3, 28,  28},
      {50, 1, 18,  32},
      {50, 3, 18,  32},
      {20, 3, 128, 128}
    };
  } else {
    shapes = {
      {1,  1, 28,  28},
      {50, 3, 18,  32},
    };
  }
  for (const TShape &shape : shapes) {
    runner.TimingTest(std::string(op_name) +  "Operator CPU",
                      false, false, kwargs, 2, 10, { shape });
  }
}

TEST(CORE_OP_RUNNER, TimingCPUBinary) {
  typedef float DType;

  const char *op_name = "elemwise_add";
  const char *backward_op_name = "_backward_add";

  const kwargs_t kwargs = test::op::CoreOpExecutor<DType>::ArgsWithOpName(
    basic_args, op_name, backward_op_name);

  test::op::CoreOperatorRunner<DType> runner;
  runner.RunBidirectional(false, { TShape({10, 10, 10, 10}) }, kwargs, 1);  // prime code and cache

  std::vector <TShape> shapes;
  if (test::performance_run) {
    shapes = {
      {1,  1, 28,  28},
      {1,  3, 28,  28},
      {50, 1, 18,  32},
      {50, 3, 18,  32},
      {20, 3, 128, 128}
    };
  } else {
    shapes = {
      {1,  1, 28,  28},
      {50, 3, 18,  32},
    };
  }
  for (const TShape &shape : shapes) {
    runner.TimingTest(std::string(op_name) + "Operator CPU", false,
                      false, kwargs, 2, 10, { shape });
  }
}

/*!
 * \brief Performance run dot product, which has different shaped inputs and outputs
 */
TEST(CORE_OP_RUNNER, TimingCPUBinaryDotProduct) {
  typedef float DType;

  const char *op_name = "dot";
  const char *backward_op_name = "_backward_dot";

  const kwargs_t kwargs = test::op::CoreOpExecutor<DType>::ArgsWithOpName(
    basic_args, op_name, backward_op_name);

  test::op::CoreOperatorRunner<DType> runner;
  runner.RunBidirectional(false, { {2, 3}, {3, 2} }, kwargs, 1);  // prime code and cache

  std::vector <TShape> shapes;
  if (test::performance_run) {
    shapes = { {28,  28}, {18,  32}, {128, 24}, {128, 256} };
  } else {
    shapes = { {28,  28}, {128, 24} };
  }
  std::vector<TShape> input_shapes(2);
  for (const TShape &shape : shapes) {
    input_shapes[0] = shape;
    input_shapes[1] = TShape({shape[1], shape[0]});
    runner.TimingTest(std::string(op_name) + " Operator CPU", false,
                      false, kwargs, 2, 10, input_shapes);
  }
}
#if MXNET_USE_CUDA == 1
TEST(CORE_OP_RUNNER, TimingGPUSimpleUnary) {
  typedef float DType;

  const char *op_name = "relu";

  const kwargs_t kwargs = test::op::CoreOpExecutor<DType>::ArgsWithOpName(basic_args, op_name);

  test::op::CoreOperatorRunner<DType> runner;
  runner.RunBidirectional(false,
                          { TShape({10, 10, 10, 10}) },
                          kwargs,
                          1);  // prime code and cache

  std::vector <TShape> shapes;
  if (test::performance_run) {
    shapes = {
      {1,  1, 28,  28},
      {1,  3, 28,  28},
      {50, 1, 18,  32},
      {50, 3, 18,  32},
      {20, 3, 128, 128}
    };
  } else {
    shapes = {
      {1,  1, 28,  28},
      {50, 3, 18,  32},
    };
  }
  for (const TShape &shape : shapes) {
    runner.TimingTest(std::string(op_name) + "Operator GPU", true, false, kwargs, 2, 10, { shape });
  }}

TEST(CORE_OP_RUNNER, TimingGPUBinary) {
  typedef float DType;

  const char *op_name = "elemwise_add";
  const char *backward_op_name = "_backward_add";

  const kwargs_t kwargs = test::op::CoreOpExecutor<DType>::ArgsWithOpName(
    basic_args, op_name, backward_op_name);

  test::op::CoreOperatorRunner<DType> runner;
  runner.RunBidirectional(true,
                          { TShape({10, 10, 10, 10}) },
                          kwargs,
                          1);  // prime code and cache

  std::vector <TShape> shapes;
  if (test::performance_run) {
    shapes = {
      {1,  1, 28,  28},
      {1,  3, 28,  28},
      {50, 1, 18,  32},
      {50, 3, 18,  32},
      {20, 3, 128, 128}
    };
  } else {
    shapes = {
      {1,  1, 28,  28},
      {50, 3, 18,  32},
    };
  }
  for (const TShape &shape : shapes) {
    runner.TimingTest(std::string(op_name) + "Operator GPU", true, false, kwargs, 2, 10, { shape });
  }
}

#endif  // MXNET_USE_CUDA == 1
