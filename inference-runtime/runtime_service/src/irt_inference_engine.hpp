// Copyright (C) 2021 Intel Corporation
//

#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <string>

#include "infer_entity.hpp"

class irt_inference_backend {								
 public:  
    explicit irt_inference_backend(const std::string& soConfigFile = {});
    ~irt_inference_backend() = default;

    inference_entity* getInferenceEntity(std::string backend);

 protected:
    int parseConfigFile(const std::string& soConfigFile);
    std::string getSoPath();
    std::string getPath(const std::string& s);

    mutable std::mutex entityMutex;
    std::map <std::string, std::string> soLibraries;
    mutable std::map <std::string, inference_entity> backendEntities;
};

