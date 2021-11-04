#pragma once

#include "lazy_tensor_core/csrc/ts_backend/TsNode.h"

namespace torch_lazy_tensors {
namespace ir {
namespace ops {

class Permute : public TsNode {
 public:
  Permute(const torch::lazy::Value& input, std::vector<int64_t> dims);

  std::string ToString() const override;

  const std::vector<int64_t>& dims() const { return dims_; }

  static lazy_tensors::Shape MakePermuteShape(
      const lazy_tensors::Shape& source_shape,
      c10::ArrayRef<int64_t> permutation);

 private:
  // The permutation of dimensions.
  std::vector<int64_t> dims_;
};

}  // namespace ops
}  // namespace ir
}  // namespace torch_lazy_tensors