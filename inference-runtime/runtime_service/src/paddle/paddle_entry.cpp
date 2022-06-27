// Copyright (C) 2021 Intel Corporation
//

#include <vector>
#include <string>
#include <iostream>

#include "infer_paddle_entity.hpp"

extern "C" void CreatInferenceEntity(std::shared_ptr<IInfer_entity>& entity) {

    entity = std::make_shared<infer_paddle_entity>();

    std::string entity_name = entity->get_entity_name();
    std::cout << "entity name is:" << entity_name << std::endl;

    return;
}

