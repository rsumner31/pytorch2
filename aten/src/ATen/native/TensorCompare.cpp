#include "ATen/ATen.h"
#include "ATen/CPUApplyUtils.h"
#include "ATen/Dispatch.h"
#include "ATen/ExpandUtils.h"
#include "ATen/NativeFunctions.h"

namespace at {
namespace native {

bool allclose(const Tensor& self, const Tensor& other, double rtol, double atol) {
  if (!self.sub(other).abs().le(other.abs().mul(rtol).add(atol)).all().toCByte()) {
    return false;
  }

  return true;
}

bool is_nonzero(const Tensor& self) {
  if (self.numel() != 1) {
    runtime_error("bool value of Tensor with more than one value is ambiguous");
  }
  Scalar localScalar = self.pImpl->localScalar();
  if (localScalar.isFloatingPoint()) {
    return localScalar.to<double>() != 0;
  } else if (localScalar.isIntegral()){
    return localScalar.to<int64_t>() != 0;
  }
  runtime_error("expected non-Tensor backed scalar");
}

template <typename scalar>
struct WhereOp {
  static void apply(Tensor& ret, const Tensor& condition, const Tensor& self, const Tensor& other) {
    CPU_tensor_apply4<scalar, uint8_t, scalar, scalar>(ret, condition, self, other,
      [](scalar& ret_val, const uint8_t& cond_val, const scalar& self_val, const scalar& other_val) {
        ret_val = cond_val ? self_val : other_val;
      }
    );
  }
};

Tensor where(const Tensor& condition, const Tensor& self, const Tensor& other) {
  if (condition.type().scalarType() != ScalarType::Byte) {
    runtime_error("Expected condition to have ScalarType Byte, but got ScalarType %s",
                  toString(condition.type().scalarType()));
  }
  Tensor b_condition, b_self, b_other;
  std::tie(b_condition, b_self, b_other) = expand_outplace(condition, self, other, "where");
  return at::_s_where(b_condition, b_self, b_other);
}

Tensor _s_where_cpu(const Tensor& condition, const Tensor& self, const Tensor& other) {
  Tensor ret = self.type().tensor(self.sizes());
  dispatch_all<void, WhereOp>(ret.type(), "where", ret, condition, self, other);
  return ret;
}

}
}
