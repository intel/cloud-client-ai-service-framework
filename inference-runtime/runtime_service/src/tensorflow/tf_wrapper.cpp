// Copyright (C) 2020 Intel Corporation
//

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <numeric>

#include "tf_wrapper.hpp"

static void buffer_free(void* data, size_t) {
    free(data);
}

static TF_Buffer* ReadBuffer(const char* file) {
    struct stat st;
    int fd = open(file, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) < 0)
        return NULL;

    TF_Buffer* buf = NULL;
    void *data = malloc(st.st_size);
    if (data) {
        if (read(fd, data, st.st_size) == st.st_size &&
            (buf = TF_NewBuffer())) {
            buf->data = data;
            buf->length = st.st_size;
            buf->data_deallocator = buffer_free;
        } else {
            free(data);
        }
    }

    close(fd);
    return buf;
}

TF_Graph *ReadBinaryProto(const char* file) {
    TF_Buffer *buf = ReadBuffer(file);
    if (!buf) return NULL;

    TF_Graph* graph = NULL;
    TF_ImportGraphDefOptions *opts;
    if ((opts = TF_NewImportGraphDefOptions())) {
        if ((graph = TF_NewGraph())) {
            TF_Status *status = TF_NewStatus();
            TF_GraphImportGraphDef(graph, buf, opts, status);
            if (TF_GetCode(status) != TF_OK) {
                TF_DeleteGraph(graph);
                graph = NULL;
            }
            TF_DeleteStatus(status);
        }
        TF_DeleteImportGraphDefOptions(opts);
    }
    TF_DeleteBuffer(buf);
    return graph;
}


TFWrapper::TFWrapper(const std::string& modelPath): mSession(nullptr) {
    // load model file
    mGraph = ReadBinaryProto(modelPath.c_str());
    if (!mGraph) return;

    TF_Status *status = TF_NewStatus();
    if (!status) return;

    // find input and output nodes
    TF_Operation* oper;
    for (size_t pos = 0; (oper = TF_GraphNextOperation(mGraph, &pos));) {
        if (!TF_OperationNumInputs(oper) &&
            strcmp(TF_OperationOpType(oper), "Const")) {
            // the interface support float input only
            if (TF_OperationOutputType({oper, 0}) == TF_FLOAT)
                mInputNodes.insert(oper);
        } else {
            int n = TF_OperationNumOutputs(oper);
            while (--n >= 0) {
                if (TF_OperationOutputNumConsumers({oper, n}) ||
                    TF_OperationOutputType({oper, n}) != TF_FLOAT)
                    break;
            }
            if (n < 0)
                mOutputNodes.insert(oper);
        }
    }

    if (!mInputNodes.empty() && !mOutputNodes.empty()) {
        // initialize session
        TF_SessionOptions *opts = TF_NewSessionOptions();
        if (opts) {
            mSession = TF_NewSession(mGraph, opts, status);
            if (mSession && TF_GetCode(status) != TF_OK) {
                TF_DeleteSession(mSession, status);
                mSession = NULL;
            }
            TF_DeleteSessionOptions(opts);
        }
    }
    TF_DeleteStatus(status);
}

TFWrapper::~TFWrapper() {
    if (mSession) {
        TF_Status *status = TF_NewStatus();
        TF_CloseSession(mSession, status);
        TF_DeleteSession(mSession, status);
        TF_DeleteStatus(status);
    }
    if (mGraph) {
        TF_DeleteGraph(mGraph);
    }
}

static void tensor_free(void* data, size_t len, void* arg) {
}
int TFWrapper::infer(const std::vector<float*>& inputData, std::vector<std::vector<float>*>& inferResults) {
    if (mSession == nullptr) return -1;
    TF_Status *status = TF_NewStatus();

    TF_Output input = { *mInputNodes.cbegin(), 0 };
    int num = TF_GraphGetTensorNumDims(mGraph, input, status);
    std::vector<int64_t> dims(num);
    TF_GraphGetTensorShape(mGraph, input, dims.data(), num, status);
    size_t size = std::accumulate(std::begin(dims), std::end(dims),
                                  sizeof(float), std::multiplies<int64_t>());
    TF_Tensor *tensor = TF_NewTensor(TF_FLOAT, dims.data(), dims.size(),
                                     inputData[0], size, tensor_free, NULL);
    if (!tensor) {
        TF_DeleteStatus(status);
        return -1;
    }

    std::vector<TF_Output> outputs;
    std::vector<TF_Tensor*> results;
    for (auto oper: mOutputNodes) {
        for (int i = 0; i < TF_OperationNumOutputs(oper); ++i) {
            TF_Output out = {oper, i};
            // the interface support float output only
            if (TF_OperationOutputType(out) == TF_FLOAT) {
                outputs.push_back({oper, i});
                results.push_back(NULL);
            }
        }
    }

    TF_SessionRun(mSession, NULL,
                  &input, &tensor, 1,
                  outputs.data(), results.data(), outputs.size(),
                  NULL, 0, NULL, status);
    auto rc = TF_GetCode(status);
    TF_DeleteTensor(tensor);
    TF_DeleteStatus(status);
    if (rc != TF_OK)
        return -1;

    for (int i = 0; i < results.size() && i < inferResults.size(); ++i) {
        std::vector<float>* outVec = inferResults[i];
        outVec->resize(TF_TensorElementCount(results[i]));
        memcpy(outVec->data(), TF_TensorData(results[i]),
               TF_TensorByteSize(results[i]));
        TF_DeleteTensor(results[i]);
    }
    return 0;
}
