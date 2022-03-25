// Copyright (C) 2022 Intel Corporation

#pragma once

#define _LIMBS_INVALID_            -1
#define _LIMBS_OK_                  0
#define _LIMBS_LEFT_OR_DOWN_        1
#define _LIMBS_RIGHT_OR_UP_         2
#define _EYE_CLOSE_                 1
#define _EYE_OPEN_                  0
#define _YAW_ANGLES_                8
#define _PITCH_ANGLES_              4
#define _ROLL_ANGLES_               0

class limbs_status {
public:
    limbs_status();
    int reset_status();
    int update_status(int status);
    int get_status() { return current_status; };
private:
    int current_status;
    int next_status;
    int count;	
};

struct human_limbs {
    bool is_corrected;
    limbs_status head;
    limbs_status shoulder;
    limbs_status body_position;
    limbs_status left_eye;
    limbs_status right_eye;
};

