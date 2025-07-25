//
//  QNNCommonExecution.cpp
//  MNN
//
//  Created by MNN on 2025/02/11.
//  Copyright © 2018, Alibaba Group Holding Limited
//

#include "QNNCommonExecution.hpp"
namespace MNN {
namespace QNN {
// #define QNN_VORBOSE
QNNCommonExecution::QNNCommonExecution(Backend *backend, const Op *op) : Execution(backend), mOp(op) {
    mBackend = (QnnBackend *)backend;
}

ErrorCode QNNCommonExecution::onResize(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) {
    this->setNodeName(mOp, inputs, outputs);

    std::string nodeNameBase = MNN::EnumNameOpType(mOp->type());
    #ifdef QNN_VORBOSE
    MNN_PRINT("%s encoding start\n", nodeNameBase.c_str());
    #endif
    ErrorCode result = this->onEncode(inputs, outputs);
    if (result != NO_ERROR) {
        MNN_ERROR("Error %s encoding\n", nodeNameBase.c_str());
        return result;
    }

    #ifdef QNN_VORBOSE
    MNN_PRINT("%s encoding end\n", nodeNameBase.c_str());
    #endif
    this->clean();

    return NO_ERROR;
}

ErrorCode QNNCommonExecution::onEncode(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) {
    return NO_ERROR;
}

ErrorCode QNNCommonExecution::onExecute(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) {
    return NO_ERROR;
}

void QNNCommonExecution::setNodeName(const Op * op, const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) {
    std::string nodeNameBase = MNN::EnumNameOpType(mOp->type());
    nodeNameBase += "_";
    std::string inputTag = "I_";
    std::string outputTag = "O_";

    for (int i = 0; i < inputs.size(); i++) {
        inputTag += std::to_string(mBackend->getTensorIdx(inputs[i]));
        inputTag += "_";
    }

    for (int j = 0; j < outputs.size() - 1; j++) {
        outputTag += std::to_string(mBackend->getTensorIdx(outputs[j]));
        outputTag += "_";
    }
    outputTag += std::to_string(mBackend->getTensorIdx(outputs[outputs.size() - 1]));

    mNodeName = nodeNameBase + inputTag + outputTag;
}

void QNNCommonExecution::createStaticTensor(const std::string & name, Qnn_DataType_t dataType, const std::vector<uint32_t> & dimensions, const void * buffer, Qnn_QuantizeParams_t quantizeParam) {
    std::string tensorName = mNodeName + "_" + name;
    std::shared_ptr<QNNTensorWrapper> tensorWrapper = QNNTensorWrapper::createStaticTensor(tensorName, dataType, dimensions, buffer, quantizeParam);
    mBackend->addStaticTensorToGraph(tensorWrapper->getNativeTensor());
    mTempTensorWrappers.push_back(tensorWrapper);
    return;
}

void QNNCommonExecution::createStaticFloatTensor(const std::string & name, Qnn_DataType_t dataType, const std::vector<uint32_t> & dimensions, const float * buffer, Qnn_QuantizeParams_t quantize) {
    std::string tensorName = mNodeName + "_" + name;
    std::shared_ptr<QNNTensorWrapper> tensorWrapper = QNNTensorWrapper::createStaticFloatTensor(tensorName, dataType, dimensions, buffer, quantize);
    mBackend->addStaticTensorToGraph(tensorWrapper->getNativeTensor());
    mTempTensorWrappers.push_back(tensorWrapper);
    return;
}

void QNNCommonExecution::createStageTensor(const std::string & name, Qnn_DataType_t dataType, const std::vector<int> & dimensions, Qnn_QuantizeParams_t quantize) {
    std::vector<uint32_t> vec(dimensions.size());
    for (int i = 0; i < dimensions.size(); i++) {
        vec[i] = (uint32_t)dimensions[i];
    }
    this->createStageTensor(name, dataType, vec, quantize);
    return;
}
void QNNCommonExecution::createStageTensor(const std::string & name, Qnn_DataType_t dataType, const std::vector<uint32_t> & dimensions, Qnn_QuantizeParams_t quantize) {
    std::string tensorName = mNodeName + "_" + name;
    std::shared_ptr<QNNTensorWrapper> tensorWrapper = QNNTensorWrapper::create(tensorName, QNN_TENSOR_TYPE_NATIVE, dataType, dimensions, quantize);
    mBackend->addStageTensorToGraph(tensorWrapper->getNativeTensor());
    mTempTensorWrappers.push_back(tensorWrapper);
    return;
}

void QNNCommonExecution::createParamTensor(const std::string & paramName, Qnn_DataType_t dataType, const std::vector<uint32_t> & dims, void * data, std::string postName) {
    MNN_ASSERT(!mNodeName.empty());
    std::string tensorName;
    if (postName.empty()) {
        tensorName = mNodeName + "_" + paramName + "_PARAM";
    } else {
        tensorName = mNodeName + "_" + paramName + "_" + postName + "_PARAM";
    }
    std::shared_ptr<QNNParamTensorWrapper> result = QNNParamTensorWrapper::create(paramName, tensorName, dataType, dims);

    void * dst = result->alloc();
    ::memcpy(dst, data, result->getNativeTensor()->v1.clientBuf.dataSize);

    mBackend->addStaticTensorToGraph(result->getNativeTensor());

    mParamTensorWrappers.push_back(result);

    return;
}

void QNNCommonExecution::createParamScalar(const std::string & name, bool data) {
    mParamScalarWrappers.push_back(QNNParamScalarWrapper::create(name, data));
    return;
}

void QNNCommonExecution::createParamScalar(const std::string & name, uint32_t data) {
    mParamScalarWrappers.push_back(QNNParamScalarWrapper::create(name, data));
    return;
}

void QNNCommonExecution::createParamScalar(const std::string & name, int data) {
    mParamScalarWrappers.push_back(QNNParamScalarWrapper::create(name, data));
    return;
}


void QNNCommonExecution::createParamScalar(const std::string & name, float data) {
    mParamScalarWrappers.push_back(QNNParamScalarWrapper::create(name, data));
    return;
}

void QNNCommonExecution::addNodeCommon(const std::vector<Tensor *> &inputs, const std::vector<Tensor *> &outputs) {
    for (int i = 0; i < mParamTensorWrappers.size(); i++) {
        mParams.push_back(*(mParamTensorWrappers[i]->getNativeParam()));
    }

    for (int j = 0; j < mParamScalarWrappers.size(); j++) {
        mParams.push_back(*(mParamScalarWrappers[j]->getNativeParam()));
    }

    for (int k = 0; k < inputs.size(); k++) {
        mInputs.push_back(*(mBackend->getNativeTensor(inputs[k])));
    }

    for (int l = 0; l < outputs.size(); l++) {
        mOutputs.push_back(*(mBackend->getNativeTensor(outputs[l])));
    }

    mBackend->addNodeToGraph(mOpConfigVersion, mNodeName.c_str(), mPackageName.c_str(), mNodeType.c_str(), mParams, mInputs, mOutputs);
}

void QNNCommonExecution::addNodeCommonPermute(const std::string & nodeNamePostfix, const Qnn_Tensor_t & input, const Qnn_Param_t & paramPerm, const Qnn_Tensor_t & output) {
    CLEAR_BEFORE_ADDING_NODE;

    std::string name = mNodeName + "_" + nodeNamePostfix;
    mNodeType = "Transpose";
    mInputs.push_back(input);
    mParams.push_back(paramPerm);
    mOutputs.push_back(output);

    mBackend->addNodeToGraph(mOpConfigVersion, name.c_str(), mPackageName.c_str(), mNodeType.c_str(), mParams, mInputs, mOutputs);
    return;
}

void QNNCommonExecution::addNodeCommonReshape(const std::string & nodeNamePostfix, const Qnn_Tensor_t & input, const Qnn_Tensor_t & output) {
    CLEAR_BEFORE_ADDING_NODE;

    std::string name = mNodeName + "_" + nodeNamePostfix;
    mNodeType = "Reshape";
    mInputs.push_back(input);
    mOutputs.push_back(output);

    mBackend->addNodeToGraph(mOpConfigVersion, name.c_str(), mPackageName.c_str(), mNodeType.c_str(), mParams, mInputs, mOutputs);
    return;
}

void QNNCommonExecution::clean() {
    mTempTensorWrappers.clear();
    mParamTensorWrappers.clear();
    mParamScalarWrappers.clear();
    mNodeName.clear();
    mNodeType.clear();
    mParams.clear();
    mInputs.clear();
    mOutputs.clear();
}

} // end namespace QNN
} // end namespace MNN
