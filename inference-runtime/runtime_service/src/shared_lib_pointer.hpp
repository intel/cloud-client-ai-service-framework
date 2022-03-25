// Copyright (C) 2021 Intel Corporation
//

#pragma once

#include <memory>
#include <string>
#include <functional>

#include "shared_lib_load.hpp"

template <class T>
class SharedLibPointer {

public:
    SharedLibPointer() = default;
    SharedLibPointer(const std::string& soName)
       : _so(soName.c_str()) {

        loadSo();
    }

    void* findFuncAddress(std::string funcName) {
        return _so.get_entry_symbol(funcName.c_str());
    }

    std::shared_ptr<T> getPointer() {
        return _ptr;
    }

protected:
    void loadSo() {
        std::string entryFunctionName = "CreatInferenceEntity";
        using EntryF = void(std::shared_ptr<T>&); //define function: void func(std::shared_ptr<T>&)
        void* p = _so.get_entry_symbol(entryFunctionName.c_str());
 
        if (p != nullptr)
            reinterpret_cast<EntryF*>(p)(_ptr); //convert void*() to void *(std::shared_ptr<T>&)
    }

   SOLibLoaded _so;
   std::shared_ptr<T> _ptr;
};
