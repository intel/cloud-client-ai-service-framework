// Copyright (C) 2020 Intel Corporation


// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <inference_engine.hpp>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <string>
#include <stdio.h>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <fcgiapp.h>
#include <fcgio.h>
#include <fcgi_stdio.h>
#include <functional>
#include <random>
#include <memory>
#include <chrono>
#include <utility>
#include <algorithm>
#include <iterator>
#include <map>
#include <sstream>
#include <unistd.h>
#include <format_reader_ptr.h>
#include <samples/ocv_common.hpp>
#include "vino_ie_pipe.hpp"
#include <ccai_log.h>
#include <sys/time.h>
#include <fstream>
#include "fcgi_utils.h"

#ifdef WITH_EXTENSIONS
#include <ext_list.hpp>
#endif

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

using namespace InferenceEngine;
using namespace std;
using namespace cv;

const int SUCCESS_STATUS = 0;
const int ERROR_STATUS = -1;


// Returns true if an UTF-8 string only consists of ASCII characters
typedef struct _RiffWaveHeader {
    unsigned int riff_tag;       /* "RIFF" string */
    int riff_length;             /* Total length */
    unsigned int  wave_tag;      /* "WAVE" */
    unsigned int  fmt_tag;       /* "fmt " string (note space after 't') */
    int  fmt_length;             /* Remaining length */
    short data_format;           /* Data format tag, 1 = PCM */
    short num_of_channels;       /* Number of channels in file */
    int sampling_freq;           /* Sampling frequency */
    int bytes_per_sec;           /* Average bytes/sec */
    short block_align;           /* Block align */
    short bits_per_sample;
    unsigned int data_tag;       /* "data" string */
    int data_length;             /* Raw data length */
} RiffWaveHeader;

uint8_t *ReadBinaryFile(const char *filename, unsigned int *size) {
    if(nullptr == filename) {
        fprintf(stderr, "Filename is NULL\n");
        return nullptr;
    }
    FILE *f = fopen(filename, "rb");
    if (f) {
        int32_t res = fseek(f, 0, SEEK_END);
        if (res != 0) {
            fprintf(stderr, "Error occured while loading file %s\n", filename);
            fclose(f);
            return nullptr;
        }
        long int file_size = ftell(f);
        if (file_size < 0) {
            fprintf(stderr, "Error occured while loading file %s\n", filename);
            fclose(f);
            return nullptr;
        }

        res = fseek(f, 0, SEEK_SET);
        if (res != 0) {
            fprintf(stderr, "Error occured while loading file %s\n", filename);
            fclose(f);
            return nullptr;
        }
        uint8_t *data = new (std::nothrow)uint8_t[file_size];
        if (data) {
            *size = fread(data, 1, file_size, f);
        }
        else {
            fprintf(stderr, "Not enough memory to load file %s\n", filename);
            fclose(f);
            return nullptr;
        }
        fclose(f);
        if (*size != file_size) {
            delete[]data;
            fprintf(stderr, "Could not read all the data from file %s\n", filename);
            return nullptr;
        }
        return data;
    }
    fprintf(stderr, "Could not open file %s\n", filename);

    return nullptr;
}

int GetWaveData(uint8_t *wave_data, unsigned int &size, short *&samples, int &sampleLength, int &bytesPerSample) {
    //const unsigned int kSubHeaderSize = 8;
    const unsigned int kFormatSize = 16;
    const short kPCMFormat = 1;
    const short kMonoStreamChannelsCount = 1;
    const int k16KHzSamplingFrequency = 16000;
    const int kBandwidthOfMono16KHz16bitStream = kMonoStreamChannelsCount *
        k16KHzSamplingFrequency * sizeof(short);
    const short k16bitSampleContainer = kMonoStreamChannelsCount * sizeof(short);
    const short kNumBitsPerByte = 8;
    const short kBitsPer16bitSample = sizeof(short) * kNumBitsPerByte;

    if (wave_data) {
        //short *samples = nullptr;
        int data_length = 0;
        RiffWaveHeader *wave_header = reinterpret_cast<RiffWaveHeader *>(wave_data);
        if (size < sizeof(RiffWaveHeader)) {
            fprintf(stderr, "Unrecognized WAVE file format - header size does not match\n");
            return ERROR_STATUS;
        }
        // make sure it is actually a RIFF file
        if (0 != memcmp(&wave_header->riff_tag, "RIFF", 4)) {
            fprintf(stderr, "The wave file is not a valid RIFF file\n");
            //delete []wave_data;
            return ERROR_STATUS;
        }
        if (0 != memcmp(&wave_header->wave_tag, "WAVE", 4)) {
            fprintf(stderr, "Unrecognized WAVE file format - required RIFF WAVE\n");
            return ERROR_STATUS;
        }
        if(0 != memcmp(&wave_header->fmt_tag, "fmt ", 4)) {
            fprintf(stderr, "Audio file format tag is incorrect\n");
            return ERROR_STATUS;
        }

        // only PCM
        if (wave_header->data_format != kPCMFormat) {
            fprintf(stderr, "Unrecognized WAVE file format - required PCM encoding\n");
            return ERROR_STATUS;
        }
        // only mono
        if (wave_header->num_of_channels != kMonoStreamChannelsCount) {
            fprintf(stderr, "Invalid channel count - required mono PCM\n");
            return ERROR_STATUS;
        }
        // only 16 bit
        if (wave_header->bits_per_sample != kBitsPer16bitSample) {
            fprintf(stderr, "Incorrect sampling resolution - required PCM 16bit sample resolution\n");
            return ERROR_STATUS;
        }
        // only 16KHz
        if (wave_header->sampling_freq != k16KHzSamplingFrequency) {
            fprintf(stderr, "Incorrect sampling rate - required 16KHz sampling rate\n");
            return ERROR_STATUS;
        }
        if (wave_header->bytes_per_sec != kBandwidthOfMono16KHz16bitStream) {
            fprintf(stderr, "Wave file doesn't have desired bytes per second (%d != %d)\n",
               wave_header->bytes_per_sec, kBandwidthOfMono16KHz16bitStream);
            return ERROR_STATUS;
        }
        if (wave_header->block_align != k16bitSampleContainer) {
            fprintf(stderr, "Wave file has unsupported block align %d required %d bits sample container\n",
               static_cast<int>(wave_header->block_align), static_cast<int>(k16bitSampleContainer));
            return ERROR_STATUS;
        }
        // make sure that data chunk follows file header
        if (0 == memcmp(&wave_header->data_tag, "data", 4)) {
            samples = reinterpret_cast<short *>(wave_data + sizeof(RiffWaveHeader));
            data_length = wave_header->data_length;
        }
        else if (( sizeof(RiffWaveHeader) + wave_header->fmt_length < size)
                 && (0 == memcmp(&wave_header->data_tag + (wave_header->fmt_length - kFormatSize), "data", 4))) {
            samples = reinterpret_cast<short *>(wave_data + sizeof(RiffWaveHeader) + (wave_header->fmt_length - kFormatSize));
            data_length = *(reinterpret_cast<int *>(wave_data + sizeof(RiffWaveHeader) + (wave_header->fmt_length - kFormatSize) - 4));
        }
        else {
            fprintf(stderr, "Unrecognized WAVE file format - header size does not match\n");
            return ERROR_STATUS;
        }

        if (reinterpret_cast<uint8_t *>(samples) + data_length > wave_data + size) {
            fprintf(stderr, "Audio file data length is incorrect\n");
            return ERROR_STATUS;
        }

        sampleLength = data_length;
        bytesPerSample = wave_header->bits_per_sample / 8;

        return SUCCESS_STATUS;
    }

    return ERROR_STATUS;
}


static bool IsASCII(std::string text) {
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

std::string asr(std::string params_str, std::string wave_filename) {
    CCAI_NOTICE("asr c++ service");


    std::string config_filename = "./models/lspeech_s5_ext/FP32/speech_lib.cfg";
    std::string ie_result = "";

    if (!IsASCII(wave_filename)) {
        ie_result = "Error: Wave filename contains non-ASCII characters\n";
        return ie_result;
    }
    if (!IsASCII(config_filename)) {
        ie_result = "Error: Configuration filename contains non-ASCII characters\n";
        return ie_result;
    }

    int sampleLength = 0;
    int bytesPerSample = 0;
    unsigned int size = 0;
    uint8_t *wave_data = ReadBinaryFile(wave_filename.c_str(), &size);
    short *samples = nullptr;
    GetWaveData(wave_data, size, samples, sampleLength, bytesPerSample);
    std::vector<char> rh_utterance_transcription(1024 * 1024);

    struct serverParams urlInfo {"https://api.ai.qq.com/fcgi-bin/aai/aai_asr", params_str};
    int res = vino_ie_pipeline_infer_speech(samples, sampleLength, bytesPerSample, config_filename,
                                            rh_utterance_transcription, urlInfo);
    if (res == RT_REMOTE_INFER_OK) {
        //------------------ get the result from  qq cloud --------------------------------

        CCAI_NOTICE("remote inference");
        CCAI_NOTICE("%s", urlInfo.response.c_str());
        int pos_start = urlInfo.response.find("{");
        int pos_end = (urlInfo.response.substr(pos_start)).rfind("}");
        ie_result += "\t\t\t";
        ie_result += urlInfo.response.substr(pos_start, pos_end);
        ie_result += ",\n";
    } else if (res == 0) {
        //--------------------------get the result locally --------------------------------

        CCAI_NOTICE("local inference");
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":0,\r\n";
        ie_result += "\t\"msg\":\"ok\",\r\n";

        ie_result += "\t\"data\":{\r\n";
        ie_result = ie_result + "\t\t\"text\":" + rh_utterance_transcription.data() + "\n";
        ie_result += "\t},\r\n";
    } else {
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":1,\r\n";
        ie_result += "\t\"msg\":\"inference error\",\r\n";
        CCAI_NOTICE("can not get inference results");

    }


    if(wave_data) {
        delete []wave_data;
    }
    return ie_result;
}

int main(int argc, char **argv) {
    int err = FCGX_Init(); /* call before Accept in multithreaded apps */
    if (err) {
        std::string log_info="FCGX_Init failed: " + std::to_string(err);
        CCAI_NOTICE("%s", log_info.c_str());
        return 1;
    }

    FCGX_Request cgi;
    err = FCGX_InitRequest(&cgi, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (err) {
        std::string log_info = "FCGX_InitRequest failed: " + std::to_string(err);
        CCAI_NOTICE("%s", log_info.c_str());
        return 2;
    }

    while (1) {
        err = FCGX_Accept_r(&cgi);
        if (err) {
            std::string log_info = "FCGX_Accept_r stopped: " + std::to_string(err);
            CCAI_NOTICE("%s", log_info.c_str());
            break;
        }
        //-----------------------check content type------------------------------
        char *pContent = FCGX_GetParam("CONTENT_TYPE", cgi.envp);
        if ((pContent == NULL) || (strstr(pContent, "application/x-www-form-urlencoded") == NULL)) {
            CCAI_NOTICE("get content error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "not Acceptable";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }
        //----------------------------get the post_data form fcgi-------------------
        struct timeval start, stop;
        gettimeofday(&start, NULL);
        double start_time = start.tv_sec + start.tv_usec / 1000000.0;
        char *post_data = NULL;
        std::string ie_result = "";
        char *pLenstr = FCGX_GetParam("CONTENT_LENGTH", cgi.envp);
        unsigned long data_length = 1;

        if (pLenstr == NULL ||
            (data_length += strtoul(pLenstr, NULL, 10)) > INT32_MAX) {
            CCAI_NOTICE("get length error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "get content length error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }

        string s_base64;

        post_data = (char *)malloc(data_length);
        if (post_data == NULL) {
            CCAI_NOTICE("malloc buffer error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "malloc buffer error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }
        int ret = 0;
        ret = FCGX_GetStr(post_data, data_length, cgi.in);
        post_data[ret] = '\0';
        std::string post_str = post_data;

        std::string result("Status: 200 OK\r\nContent-Type: text/html\r\ncharset: utf-8\r\n"
                           "X-Content-Type-Options: nosniff\r\nX-frame-options: deny\r\n\r\n");

        std::string sub_str = "";
        sub_str = get_data(post_str, "healthcheck");
        if (sub_str == "1") {
            result = result + "healthcheck ok";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        else{

            sub_str = get_data(post_str, "speech");
            s_base64 = deData(sub_str);
            s_base64 = DecodeBase(s_base64.data(), s_base64.size());

            std::string wav_name;
            wav_name = "/tmp/test_" + std::to_string(start.tv_sec) +"_" + std::to_string(start.tv_usec) + ".wav";
            ofstream f1(wav_name);
            if (f1.is_open()) {
                f1 << s_base64;
                f1.close();
            }


            //-----------------------------do inference and get the result ------------------
            ie_result = asr(post_str, wav_name);

            //-----------------------------prepare output -----------------------------
            gettimeofday(&stop, NULL);
            double stop_time = stop.tv_sec + stop.tv_usec / 1000000.0;
            result += ie_result;
            double time = stop_time - start_time;
            std::string time_str = std::to_string(time);
            int pos_start = time_str.find(".");
            std::string cut_time_str = time_str.substr(0, pos_start) + time_str.substr(pos_start, 4);
            result += "\t\"time\":" + cut_time_str + "\n}";
            remove(wav_name.c_str());

            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        //------------------free memory----------------------

        free(post_data);
    }

    return 0;
}
