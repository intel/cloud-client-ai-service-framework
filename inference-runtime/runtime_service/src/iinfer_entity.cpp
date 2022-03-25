// Copyright (C) 2021 Intel Corporation
//

#include <iostream>

#include "iinfer_entity.hpp"

std::string IInfer_entity::get_entity_name() const noexcept {
    return _inferEntityName;
}

void IInfer_entity::set_entity_name(const std::string& name) noexcept {
    _inferEntityName = name;
}

void IInfer_entity::set_config(const std::map<std::string, std::string>& config) {
    _config = config;
}

void IInfer_entity::get_config() {
    std::cerr << "Not implemented!" << std::endl;
}
