// Copyright (C) 2021 Intel Corporation
//

#include <dlfcn.h>
#include <iostream>

#include "shared_lib_load.hpp"

class SOLibLoaded::Impl {
public:
    explicit Impl(const char* backendSOName) {
        soLibrary = dlopen(backendSOName, RTLD_NOW);
        if (soLibrary == nullptr)
            std::cerr << "Cannot load so library '" << backendSOName << "': " << dlerror() << std::endl;
        else
            std::cout << "open so lib ok: " << backendSOName << std::endl;
    }

    ~Impl() {
        if ((soLibrary != nullptr) && (dlclose(soLibrary) != 0)) {
            std::cerr << "dlclose failed: " << dlerror() << std::endl;
        }
    }

    void* find_symbol(const char* functionName) const {
        void* funcAddr = nullptr;

        if (soLibrary != nullptr)
            funcAddr = dlsym(soLibrary, functionName);

        if (funcAddr == nullptr)
            std::cerr << "dlSym cannot find function symbol '" << functionName << "': " << dlerror() << std::endl;

        return funcAddr;
    }

private:
    void* soLibrary = nullptr;

};

SOLibLoaded::SOLibLoaded(const char* backendEngineName) {
    _impl.reset(new Impl(backendEngineName));
}

void* SOLibLoaded::get_entry_symbol(const char* entryFunctionName) const {
    if (_impl == nullptr) {
        std::cerr << "SOLibLoaded is not initialized!" << std::endl;
        return nullptr;
    }
    return _impl->find_symbol(entryFunctionName);
}
	
