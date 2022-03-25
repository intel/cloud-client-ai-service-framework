// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <tensorflow/c/c_api.h>

class TFWrapper {
public:
    TFWrapper() {};
    TFWrapper(const std::string& modelPath);
    ~TFWrapper();

    int infer(const std::vector<float*>& inputData, std::vector<std::vector<float>*>& inferResults);

private:
    TF_Session* mSession;
    TF_Graph *mGraph;
    std::unordered_set<TF_Operation*> mInputNodes;
    std::unordered_set<TF_Operation*> mOutputNodes;
};
