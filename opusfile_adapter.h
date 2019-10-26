#pragma once

#include <string>

namespace tg_rate {
    bool opus_decode(const std::string& input_filename, const std::string& decoded_wav_file_name);
    bool opus_decode_mono_16khz(const std::string& input_filename, const std::string& decoded_wav_file_name);
}