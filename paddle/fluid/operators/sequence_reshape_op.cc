//   Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserve.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "paddle/fluid/operators/sequence_reshape_op.h"
#include "paddle/fluid/framework/ddim.h"

namespace paddle {
namespace operators {

class SequenceReshapeOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;
  void InferShape(framework::InferShapeContext* ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("X"),
                   "Input(X) of SequenceReshapeOp should not be null.");
    PADDLE_ENFORCE(ctx->HasOutput("Out"),
                   "Output(Out) of SequenceReshapeOp should not be null.");
    auto x_dims = ctx->GetInputDim("X");
    auto x_numel = product(x_dims);
    PADDLE_ENFORCE_EQ(x_dims.size(), 2U, "Rank of Input(X) should be 2.");
    int new_dim = ctx->Attrs().Get<int>("new_dim");
    if (ctx->IsRuntime()) {
      ctx->SetOutputDim("Out",
                        {x_numel / new_dim, static_cast<int64_t>(new_dim)});
    } else {
      // when compiling, the batch size is undetermined, just set to -1
      ctx->SetOutputDim("Out", {-1, static_cast<int64_t>(new_dim)});
    }
  }
};

class SequenceReshapeOpMaker : public framework::OpProtoAndCheckerMaker {
 public:
  SequenceReshapeOpMaker(OpProto* proto, OpAttrChecker* op_checker)
      : OpProtoAndCheckerMaker(proto, op_checker) {
    AddInput("X",
             "(LoDTensor, default LoDTensor<float>) A 2-D LoDTensor with shape "
             "being [N, M].");
    AddOutput("Out",
              "(LoDTensor, default LoDTensor<float>) A 2-D LoDTensor with "
              "shape [T, new_dim] where T is calculated based on X.lod, M and "
              "new_dim.");
    AddAttr<int>("new_dim", "Sequence dimension of the output LoDTensor.");
    AddComment(R"DOC(
Sequence Reshape Operator.

This operator will rearrange the input sequences. The new dimension is set by
attribute and length of each sequence may change longer or shorter which is
decided by original length, original dimension and new dimension. The following
example will help to illustrate the function of this operator:

x is a LoDTensor:
    x.lod  = [[0, 2, 6]]
    x.data = [[1, 2], [3, 4],
              [5, 6], [7, 8], [9, 10], [11, 12]]
    x.dims = [6, 2]

set new_dim = 4

then out is a LoDTensor:
    out.lod  = [[0, 1, 3]]
    out.data = [[1, 2, 3, 4],
                [5, 6, 7, 8], [9, 10, 11, 12]]
    out.dims = [3, 4]

Currently, only 1-level LoDTensor is supported and please make sure (original
length * original dimension) can be divided by new_dim with no remainder for
each sequence.

)DOC");
  }
};

class SequenceReshapeGradOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

  void InferShape(framework::InferShapeContext* ctx) const override {
    PADDLE_ENFORCE(
        ctx->HasInput(framework::GradVarName("Out")),
        "Input(Out@GRAD) of SequenceReshapeGradOp should not be null.");
    PADDLE_ENFORCE(ctx->HasInput("X"),
                   "Input(X) of SequenceReshapeGradOp should  not be null.");

    ctx->SetOutputDim(framework::GradVarName("X"), ctx->GetInputDim("X"));
    ctx->ShareLoD("X", /*->*/ framework::GradVarName("X"));
  }
};

class SequenceReshapeGradOpMaker : public framework::SingleGradOpDescMaker {
 public:
  using framework::SingleGradOpDescMaker::SingleGradOpDescMaker;

 protected:
  std::unique_ptr<framework::OpDesc> Apply() const override {
    auto* op_desc_ptr = new framework::OpDesc();
    op_desc_ptr->SetType("sequence_reshape_grad");
    op_desc_ptr->SetInput("X", Input("X"));
    op_desc_ptr->SetInput(framework::GradVarName("Out"), OutputGrad("Out"));
    op_desc_ptr->SetOutput(framework::GradVarName("X"), InputGrad("X"));
    op_desc_ptr->SetAttrMap(Attrs());
    return std::unique_ptr<framework::OpDesc>(op_desc_ptr);
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;
REGISTER_OPERATOR(sequence_reshape, ops::SequenceReshapeOp,
                  ops::SequenceReshapeOpMaker, ops::SequenceReshapeGradOpMaker);
REGISTER_OPERATOR(sequence_reshape_grad, ops::SequenceReshapeGradOp);
REGISTER_OP_CPU_KERNEL(
    sequence_reshape,
    ops::SequenceReshapeKernel<paddle::platform::CPUDeviceContext, float>,
    ops::SequenceReshapeKernel<paddle::platform::CPUDeviceContext, double>,
    ops::SequenceReshapeKernel<paddle::platform::CPUDeviceContext, int>,
    ops::SequenceReshapeKernel<paddle::platform::CPUDeviceContext, int64_t>);
REGISTER_OP_CPU_KERNEL(
    sequence_reshape_grad,
    ops::SequenceReshapeGradKernel<paddle::platform::CPUDeviceContext, float>,
    ops::SequenceReshapeGradKernel<paddle::platform::CPUDeviceContext, double>,
    ops::SequenceReshapeGradKernel<paddle::platform::CPUDeviceContext, int64_t>,
    ops::SequenceReshapeGradKernel<paddle::platform::CPUDeviceContext, int>);
