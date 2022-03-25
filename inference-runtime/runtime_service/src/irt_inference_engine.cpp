// Copyright (C) 2021 Intel Corporation
//

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <unistd.h>
#include <dlfcn.h>

#include <syslog.h>

#include "irt_inference_engine.hpp"

static bool checkASCII(std::string text) {

    for (std::string::const_iterator it = text.cbegin(); it != text.cend(); ++it) {
        const unsigned char kMinASCII = 1;
        const unsigned char kMaxASCII = 127;

        unsigned char character = static_cast<unsigned char>(*it);
        if (character < kMinASCII || character > kMaxASCII) {
            return false;
        }
    }

    return true;
}

irt_inference_backend::irt_inference_backend(const std::string& soConfigFile) {

    parseConfigFile(soConfigFile);
}

inference_entity* irt_inference_backend::getInferenceEntity(std::string backend) {
    std::lock_guard<std::mutex> lock(entityMutex);

    auto it = soLibraries.find(backend);
    if (it == soLibraries.end()) {
        syslog(LOG_ERR, "AI-Service-Framework: this backend not registed! \n");
        return nullptr;
    }

    if (backendEntities.find(backend) == backendEntities.end()) {
        std::string soFile = it->second;
        std::cout << "so file:" << soFile << std::endl;

        inference_entity entity{soFile};
        backendEntities[backend] = entity;     
    }

    return &backendEntities[backend];
}

int irt_inference_backend::parseConfigFile(const std::string& soConfigFile) {

    std::lock_guard<std::mutex> lock(entityMutex);

    std::string listSoFile = soConfigFile;
    std::string path = getSoPath();
    std::cout << "get so path is: " << path << std::endl;
    if (path.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: not find share lib(so)! \n");
        return -1;
    }

    if (listSoFile.empty()) {
        std::cout << "so configuration file is empty!" << std::endl;

        listSoFile = "/etc/inference_engine_library.txt";
        if (access(listSoFile.c_str(), 0) != 0) { // mode = 0: whether file exist
            syslog(LOG_ERR, "AI-Service-Framework: no configuration file under /etc/, try another path! \n");
            listSoFile = path + "/inference_engine_library.txt";
            if (access(listSoFile.c_str(), 0) != 0) {
                syslog(LOG_ERR, "AI-Service-Framework: configuration file doesn't exist! \n");
                return -1;
            }
        }
    }

    std::ifstream file(listSoFile.c_str());
    if (!file.is_open()) {
        syslog(LOG_ERR, "AI-Service-Framework: Failed to open configuration file! \n");
        return -1;
    }

    std::string line;
    while (std::getline(file, line)) {

        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) {
            return !std::isspace(ch);}));

        // empty line and comment is a valid case
        if (line.empty() || line.rfind("#", 0) == 0) {
            continue;
        }

        if (!checkASCII(line)) {
            syslog(LOG_ERR, "AI-Service-Framework: found non-ASCII character in configuration file. \n");
            return -1;
        }

        const char COLUMN_SEPARATOR = ' ';
        if (line.find(COLUMN_SEPARATOR) == std::string::npos) {
            syslog(LOG_ERR, "AI-Service-Framework: Invalid format of configuration file! \n");
            return -1;
        }

        std::string param_name, param_value;

        std::size_t nPos = line.find(COLUMN_SEPARATOR);
        param_name = line.substr(0, nPos);
        line.erase(0, nPos);

        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) {
            return !std::isspace(ch);}));

        nPos = line.find(COLUMN_SEPARATOR);
        param_value = line.substr(0, nPos);

        if (param_name.empty())
        {
            syslog(LOG_ERR, "AI-Service-Framework: Invalid format of configuration file, empty name! \n");
            return -1;
        }

        soLibraries[param_name] = path + '/' + param_value;
     }

    file.close();

    return 0;
}

std::string irt_inference_backend::getPath(const std::string& s) {
    if (s.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: path empty! \n");
        return {};
    }

    size_t i = s.rfind("/", s.length());
    if (i != std::string::npos) {
        return (s.substr(0, i));
    } else {
        syslog(LOG_NOTICE, "AI-Service-Framework: same path! \n");
        return "."; // the current folder
    }
}

std::string irt_inference_backend::getSoPath() {

    Dl_info info;

    int res = dladdr(reinterpret_cast<void*>(checkASCII), &info);
    if (!res) {
        syslog(LOG_ERR, "AI-Service-Framework: not find function symbol! \n");
        return {};
    } else {
        std::string so_path = getPath(std::string(info.dli_fname));
        syslog(LOG_NOTICE, "AI-Service-Framework: path=%s \n", so_path.c_str());

        return so_path;
    }
} 
