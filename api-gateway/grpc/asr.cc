// Copyright (C) 2020 Intel Corporation

#include <string>
#include <opencv2/opencv.hpp>
#include "vino_ie_pipe.hpp"
#include "asr.h"
#include "grpc_inference_service.h"

#define MODEL_CONFIG_FILE	"models/lspeech_s5_ext/FP32/speech_lib.cfg"

typedef struct _wave_header_t {
	uint32_t tag;                /* "RIFF" string */
	int32_t size;                /* total length */
	uint32_t wave_tag;           /* "WAVE" */
	uint32_t fmt_tag;            /* "fmt " string (note space after 't') */
	int32_t fmt_size;            /* remaining length */
	int16_t format;              /* data format tag, 1 = PCM */
	int16_t num_of_channels;     /* number of channels in file */
	int32_t freq;                /* sampling frequency */
	int32_t bytes_per_sec;       /* average bytes/sec */
	int16_t align;               /* block align */
	int16_t bits_per_sample;
	uint32_t data_tag;           /* "data" string */
	int32_t data_size;           /* data length */
} wave_header_t;

bool get_wave_data(const char *wave, uint32_t size,
		   int16_t **samples, int *samples_len, int *bytes_pre_sample)
{
	uint32_t format_size = 16;
	int16_t pcm_format = 1;
	int16_t channels_count = 1;
	int frequency = 16000;
	int bandwidth = channels_count * frequency * sizeof(int16_t);
	int16_t sample_container = channels_count * sizeof(int16_t);
	int16_t bits_per_sample = sizeof(int16_t) * 8;
	int data_len = 0;

	const wave_header_t *header = (const wave_header_t *)wave;
	if (size < sizeof(wave_header_t)) {
		error_log("Unrecognized WAVE file format - header size\n");
		return false;
	}
	// make sure it is actually a RIFF file
	if (memcmp(&header->tag, "RIFF", 4) != 0) {
		error_log("The wave file is not a valid RIFF file\n");
		return false;
	}
	if (memcmp(&header->wave_tag, "WAVE", 4) != 0) {
		error_log("Unrecognized WAVE file format\n");
		return false;
	}
	if (memcmp(&header->fmt_tag, "fmt ", 4) != 0) {
		error_log("Audio file format tag is incorrect\n");
		return false;
	}
	// only PCM
	if (header->format != pcm_format) {
		error_log("Unrecognized WAVE file format\n");
		return false;
	}
	// only mono
	if (header->num_of_channels != channels_count) {
		error_log("required mono PCM\n");
		return false;
	}
	// only 16 bit
	if (header->bits_per_sample != bits_per_sample) {
		error_log("required PCM 16bit sample resolution\n");
		return false;
	}
	// only 16KHz
	if (header->freq != frequency) {
		error_log("required 16KHz sampling rate\n");
		return false;
	}
	if (header->bytes_per_sec != bandwidth) {
		error_log( "doesn't have desired bytes per second (%d != %d)\n",
			   header->bytes_per_sec, bandwidth);
		return false;
	}
	if (header->align != sample_container) {
		error_log("block align %d required %d bits sample container\n",
			  static_cast<int>(header->align),
			  static_cast<int>(sample_container));
		return false;
	}
	// make sure that data chunk follows file header
	if (memcmp(&header->data_tag, "data", 4) == 0) {
		*samples = (int16_t *)(wave + sizeof(wave_header_t));
		data_len = header->data_size;
	} else if ((sizeof(wave_header_t) + header->fmt_size < size)
		   && (memcmp(&header->data_tag +
			      (header->fmt_size - format_size),
			      "data", 4) == 0)) {
		*samples = (int16_t *)(wave + sizeof(wave_header_t) + \
				       (header->fmt_size - format_size));
		data_len = *(int *)(wave + sizeof(wave_header_t) + \
				    (header->fmt_size - format_size) - 4);
	} else {
		error_log("header size does not match\n");
		return false;
	}
	if ((char *)(*samples) + data_len > wave + size) {
		error_log("Audio file data length is incorrect\n");
		return false;
	}

	*samples_len = data_len;
	*bytes_pre_sample = header->bits_per_sample / 8;

	return true;
}

std::string asr(std::string input_buffer)
{
	int len;
	int bytes_pre_sample;
	int16_t *samples;
	std::string ie_result("");

	if (get_wave_data(input_buffer.c_str(),
			  input_buffer.size(),
			  &samples, &len, &bytes_pre_sample) == false)
		return std::string("");

	// vino parameters
	std::vector<char> buf(1024 * 1024);
	std::string param("");
	struct serverParams url_info {
		"https://api.ai.qq.com/fcgi-bin/aai/aai_asr", param
	};

	int ret = vino_ie_pipeline_infer_speech(samples, len, bytes_pre_sample,
						MODEL_CONFIG_FILE,
						buf, url_info);
	if (ret == RT_REMOTE_INFER_OK || ret == RT_INFER_ERROR) {
		error_log("error or decode remove infer results\n");
		return ie_result;
	}

	std::string text(buf.data());
	while (text.size() > 0) {
		if (text.back() != '\n' && text.back() != '\0')
			break;
		text.pop_back();
	}

	ie_result += "{\"text\":\"" + text + "\"}";

	return ie_result;
}
