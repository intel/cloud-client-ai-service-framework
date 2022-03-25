// Copyright (C) 2021 Intel Corporation
//

#pragma once

#include <memory>

class SOLibLoaded {
    class Impl;
    std::shared_ptr<Impl> _impl;

 public:
    SOLibLoaded() = default;
    explicit SOLibLoaded(const char* backendEngineName);
    ~SOLibLoaded() {};
    void* get_entry_symbol(const char* entryFunctionName) const;
};
