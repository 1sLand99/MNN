//
//  ConvBufLowMemoryExecution.hpp
//  MNN
//
//  Created by MNN on 2023/10/12.
//  Copyright © 2018, Alibaba Group Holding Limited
//

#ifdef MNN_LOW_MEMORY
#ifndef MNN_OPENCL_BUFFER_CLOSED
#ifndef ConvBufLowMemoryExecution_hpp
#define ConvBufLowMemoryExecution_hpp
#include "core/ConvolutionCommon.hpp"
#include "ConvBufExecution.hpp"

namespace MNN {
namespace OpenCL {

class ConvBufLowMemoryExecution : public ConvBufCommonExecution, public CommonExecution {
public:
    ConvBufLowMemoryExecution(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs, const MNN::Op *op, Backend *backend);
    ConvBufLowMemoryExecution(std::shared_ptr<ConvBufResource> resource, const MNN::Op* op, Backend* backend);
    virtual ~ConvBufLowMemoryExecution();
    virtual ErrorCode onResize(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) override;
    virtual ErrorCode onExecute(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) override;
    virtual bool onClone(Backend* bn, const Op* op, Execution** dst) override;
private:
    int getExecuteTime();
    void getInfoFromOpLowMemory(void *weight_ptr);
    void set1x1WeightLowMemory();
    void setGeneralWeightLowMemory();
    void tuneGeneralCaseLowMemory(Tensor * input, Tensor * output);
	void useFPWeightGemmLowMemory(Tensor * input, Tensor * output);
    void tuneGemvLowMemory(Tensor * input, Tensor * output);
    void tuneGemmLowMemory(Tensor * input, Tensor * output);
    bool convertToQuantWeight1x1Buffer(cl::Buffer input);
    std::vector<int> mPaddings{0, 0};
    std::vector<uint32_t> mGlobalWorkSize{1, 1, 1};
    std::vector<uint32_t> mLocalWorkSize{1, 1, 1, 1};
    void *mFilterDataPtr = nullptr;
    bool mUseFPWeight = false;
    std::shared_ptr<Tensor> mConvGemmInpTensor;
    std::shared_ptr<Tensor> mConvGemmOutTensor;
    std::shared_ptr<Tensor> mConvGemmWeightTensor;
    std::shared_ptr<KernelWrap> mBufferToConv1x1Kernel = nullptr;
    uint32_t batchConvMode = 0; // batch > 1 convolution input arrage mode. 0 is need tune; 1 arrage to n/4chw4; 2 arrage to c/4hwn4
    std::shared_ptr<StrassenMatrixComputor> mStrassenComputor;
};

} // namespace OpenCL
} // namespace MNN
#endif /* ConvBufLowMemoryExecution_hpp */
#endif /* MNN_OPENCL_BUFFER_CLOSED */
#endif /* MNN_LOW_MEMORY */
