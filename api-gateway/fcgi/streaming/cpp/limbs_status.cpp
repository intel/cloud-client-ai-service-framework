// Copyright (C) 2022 Intel Corporation

#include "limbs_status.hpp"

limbs_status::limbs_status() {
    current_status = _LIMBS_INVALID_; 
    next_status = _LIMBS_INVALID_;
    count = 0;
}

int limbs_status::reset_status() {
    current_status = _LIMBS_INVALID_; 
    next_status = _LIMBS_INVALID_;
    count = 0;

    return 0;
}

int limbs_status::update_status(int status) {

    if (status == next_status) {
        count++;
    } else {
        next_status = status;
	count = 1;
    }

    if (count >= 3) {
       //should use lock
       current_status = next_status;
       count = 0;
    }

    return 0;
}

