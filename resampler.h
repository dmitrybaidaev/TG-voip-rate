#pragma once

#include <string>

namespace tg_rate {
    bool resample(const std::string& in_file, const std::string& out_file, size_t out_sample_rate, size_t out_channels);
}