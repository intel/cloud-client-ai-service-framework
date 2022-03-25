// Copyright (C) 2020 Intel Corporation
#include "fcgi_utils.h"

#ifdef WITH_EXTENSIONS
#include <ext_list.hpp>
#endif

std::string EncodeBase(const unsigned char* input_data, int input_num) {
    const char encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char batch_input[4] = { 0 };
    std::string encode_str;
    int length = 0;
    for (int i = 0; i < (int)(input_num / 3); i++) {
        batch_input[1] = *input_data++;
        batch_input[2] = *input_data++;
        batch_input[3] = *input_data++;
        encode_str += encode_table[batch_input[1] >> 2];
        encode_str += encode_table[((batch_input[1] << 4) | (batch_input[2] >> 4)) & 0x3F];
        encode_str += encode_table[((batch_input[2] << 2) | (batch_input[3] >> 6)) & 0x3F];
        encode_str += encode_table[batch_input[3] & 0x3F];

        length +=4;
        if (length == 76) {
            encode_str += "\r\n";
            length = 0;
        }
    }

    int mod = input_num % 3;
    if (mod == 2) {
        batch_input[1] = *input_data++;
        batch_input[2] = *input_data++;
        encode_str += encode_table[(batch_input[1] & 0xFC) >> 2];
        encode_str += encode_table[((batch_input[1] & 0x03) << 4) | ((batch_input[2] & 0xF0) >> 4)];
        encode_str += encode_table[((batch_input[2] & 0x0F) << 2)];
        encode_str += "=";
    } else {
        batch_input[1] = *input_data++;
        encode_str += encode_table[(batch_input[1] & 0xFC) >> 2];
        encode_str += encode_table[((batch_input[1] & 0x03) << 4)];
        encode_str += "==";
    }

    return encode_str;
}

std::string DecodeBase(const char *base64_data, int size){
    static const uint32_t table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        62, // '+'
        0, 0, 0,
        63, // '/'
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
        0, 0, 0, 0, 0, 0, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
        0, 0, 0, 0, 0, 0,
        26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // 'a'-'z'
    };

    std::string s_decode;
    int i = 0;
    for (;;) {
        // get four 6bit(a char), a group
        uint8_t code[4];
        int j = 0;
        while (i < size && j < 4) {
            uint8_t c = base64_data[i++];
            if (c == '\r' || c == '\n')
                continue;
            code[j++] = c;
        }
        if (j != 4)
            break;

        // decode to 3 chars
        uint32_t value = (table[code[0]] << 18) + (table[code[1]] << 12);
        s_decode += (value & 0x00FF0000) >> 16;
        if (code[2] == '=')
            continue;
        value += table[code[2]] << 6;
        s_decode += (value & 0x0000FF00) >> 8;

        if (code[3] == '=')
            continue;
        value += table[code[3]];
        s_decode += value & 0x000000FF;
    }
    return s_decode;
}

cv::Mat Base2Mat(std::string &s_base64) {
    cv::Mat img;
    std::string s_mat;
    s_mat = DecodeBase(s_base64.data(), s_base64.size());
    std::vector<char> base64_img(s_mat.begin(), s_mat.end());
    img = cv::imdecode(base64_img, 1);
    return img;
}

std::string Base2string(std::string &s_base64) {
    std::string s_mat;
    s_mat = DecodeBase(s_base64.data(), s_base64.size());
    return s_mat;
}

short int Char_dec(char data) {
    if ('0' <= data && data <= '9') {
        return short(data - '0');
    } else if ('a' <= data && data <= 'f') {
        return ( short(data -'a') + 10);
    } else if ('A' <= data && data <= 'F') {
        return (short(data -'A') + 10);
    } else {
        return -1;
    }
}

std::string deData(const std::string &s_base64) {
    std::string result = "";
    for (size_t i = 0; i < s_base64.size(); i++) {
        char data = s_base64[i];
        if (data != '%') {
            result += data;
        } else {
            int number = 0;
            char data1 = s_base64[++i];
            char data0 = s_base64[++i];
            number += Char_dec(data1) * 16 + Char_dec(data0);
            result += char(number);
        }
    }
    return result;
}


std::string get_data(std::string post_str, std::string name_str ) {
    size_t start_pos, end_pos;
    std::string sub_str = "";

    name_str += "=";
    start_pos = post_str.find(name_str);
    if (start_pos == std::string::npos) {
        return sub_str;
    }

    sub_str = post_str.substr(start_pos);
    end_pos = sub_str.find("&");
    if (end_pos != std::string::npos) {
        sub_str = post_str.substr(start_pos + name_str.length(), end_pos - name_str.length());
    } else {
        sub_str = post_str.substr(start_pos + name_str.length());
    }
    return sub_str;
}
