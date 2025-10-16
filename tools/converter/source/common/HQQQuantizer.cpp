#include "HQQQuantizer.hpp"
#include <iostream>
#include <numeric>
#include <limits>
#include <chrono>
using namespace MNN::Express;
namespace MNN {
namespace Quantization {

static VARP _shrink(VARP X, float beta, float lp_norm) {
    auto out = _Abs(X);
    if (lp_norm == 1.0f) {
        // torch.sign(x) * torch.nn.functional.relu(torch.abs(x) - 1.0 / beta)
        out = out - _Scalar<float>(1.0f/beta);
        out = _Relu(out);
        out = out * _Sign(X);
        return out;
    }
    //torch.sign(x) * torch.nn.functional.relu(torch.abs(x) - (1.0 / beta) * torch.pow(torch.abs(x), lp_norm - 1))
    auto o1 = _Pow(_Relu6(out, 0.00000001f, std::numeric_limits<float>::max()), _Scalar<float>(lp_norm-1.0f));
    auto o0 = _Relu(out - _Scalar<float>(1.0f/beta) * o1);
    out = o0 * _Sign(X);
    return out;
}

/*
def _optimize_weights_proximal_legacy_step(self, W_f, scale, zero, min_max, beta, lp_norm, axis):
    W_q = torch.round(W_f * scale + zero).clamp_(min_max[0], min_max[1])
    W_r = (W_q - zero) / scale
    W_e = self._shrink_lp_op(W_f - W_r, beta, lp_norm)
    zero = torch.mean(W_q - (W_f - W_e) * scale, axis=axis, keepdim=True)
    return scale, zero
*/
static std::pair<VARP, VARP> _optimize_weights_proximal_legacy_step(VARP W_f, VARP scale, VARP zero, float minW, float maxW, float beta, float lp_norm) {
    auto WQ = _Relu6(_Round(W_f * scale + zero), minW, maxW);
    auto WR = (WQ - zero) * _Reciprocal(scale);
    auto WE = _shrink(W_f-WR, beta, lp_norm);
    auto newZero = _ReduceMean(WQ-(W_f-WE)*scale, {1}, true);
    Variable::compute({WQ, WR, WE, newZero});
    WR.fix(VARP::CONSTANT);
    WE.fix(VARP::CONSTANT);
    newZero.fix(VARP::CONSTANT);

    return std::make_pair(scale, newZero);
}

HQQQuantizer::HQQQuantizer(const QuantizationConfig& config) : mConfig(config) {
    // 验证配置参数
    MNN_ASSERT(mConfig.bits > 0 && mConfig.bits <= 8);
    MNN_ASSERT(mConfig.group_size > 0);
}

HQQQuantizer::QuantizationResult HQQQuantizer::quantize(
    const std::vector<float>& weights) {
    
    QuantizationResult result;
    result.config = mConfig;
    
    int total_elements = weights.size();
    result.elementSize = total_elements;

    // 计算分组数量
    int num_groups = (total_elements + mConfig.group_size - 1) / mConfig.group_size;
    std::shared_ptr<MNN::Tensor> wrap(Tensor::create<float>({num_groups, mConfig.group_size}, (void*)weights.data()));
    auto W = Variable::create(Express::Expr::create(wrap.get(), false));
    auto minW = _ReduceMin(W, {1}, true);
    auto maxW = _ReduceMax(W, {1}, true);
    int qmin = 0;
    int qmax = (1 << (mConfig.bits)) - 1;
    auto threadHold = _Scalar<float>(1.0f / (float)(qmax - qmin));
    auto scale = _Relu6((maxW-minW)*threadHold, 0.0000001f, std::numeric_limits<float>::max());
    auto scaleRev = _Scalar<float>(1.0f) / scale;
    auto zero = scaleRev * _Negative(minW);
    Variable::compute({scaleRev, zero, W});
    scaleRev.fix(VARP::CONSTANT);
    zero.fix(VARP::CONSTANT);
    if (mConfig.optimize) {
        optimize(scaleRev, zero, W);
    }
    // Turn uint -> int, sub 1<<(bit-1)
    scale = _Reciprocal(scaleRev);
    minW = _Negative(zero * scale);
    qmin = -(1 << (mConfig.bits-1));
    qmax = (1 << (mConfig.bits-1)) - 1;
    auto qw = (W - minW) * scaleRev + _Scalar<float>((float)qmin);
    qw = _Relu6(_Round(qw), qmin, qmax);
    qw = _Cast<int8_t>(qw);
    result.SZ = _Concat({minW, scale}, -1);
    result.QW = qw;
    Variable::compute({result.SZ, result.QW});
    result.SZ.fix(VARP::CONSTANT);
    result.QW.fix(VARP::CONSTANT);

    return result;
}

void HQQQuantizer::optimize(MNN::Express::VARP& S, MNN::Express::VARP& Z, VARP WF) {
    int qmin = 0;
    int qmax = (1 << (mConfig.bits)) - 1;
    for (int i=0; i<mConfig.iters; ++i) {
        auto sz = _optimize_weights_proximal_legacy_step(WF, S, Z, qmin, qmax, mConfig.beta, mConfig.lp_norm);
        S = sz.first;
        Z = sz.second;
    }
}


MNN::Express::VARP HQQQuantizer::dequantize(const QuantizationResult& result) {
    auto sz = _Unstack(result.SZ, -1);
    auto S = _Unsqueeze(sz[1], {1});
    auto Z = _Unsqueeze(sz[0], {1});
    int qmin = -(1 << (mConfig.bits-1));
    int qmax = (1 << (mConfig.bits-1)) - 1;
    auto qw = _Cast<float>(result.QW);
    auto W = (qw - _Scalar<float>(qmin)) * S + Z;
    W.fix(VARP::CONSTANT);
    return W;
}



} // namespace Quantization
} // namespace AliNN
