#include <stdio.h>
#include <string>
#include "opusfile_adapter.h"
#include "PESQ-MOS/pesq.h"
#include "rate_log.h"

namespace {
    void usage() {
        fprintf(stdout, "Usage:\n");
        fprintf(stdout, "tgvoiprate path_to_sound_A.opus path_to_sound_B.opus\n");
    }
}

extern "C" {
    void pesq_measure(SIGNAL_INFO * ref_info, SIGNAL_INFO * deg_info, ERROR_INFO * err_info, long * Error_Flag, char ** Error_Type);
    void init_resamplers(long sample_rate);
}


int main(int argc, const char **argv)
{
    if(argc != 3) {
        usage();
        return 1;
    }
    std::string test_filename = std::string(argv[0]);
    std::string input_filename_A = std::string(argv[1]);
    std::string input_filename_B = std::string(argv[2]);

    if(input_filename_A.empty() || input_filename_B.empty()) {
        usage();
        return 1;
    }

    const std::string in_wav_A = input_filename_A + "_mono_16khz.wav";
    const std::string in_wav_B = input_filename_B + "_mono_16khz.wav";

    if(!tg_rate::opus_decode_mono_16khz(input_filename_A, in_wav_A)) {
        RATE_LOGE("failed to decode Opus file: %s\n", input_filename_A.c_str());
        return 1;
    }

    if(!tg_rate::opus_decode_mono_16khz(input_filename_B, in_wav_B)) {
        RATE_LOGE("failed to decode Opus file: %s\n", input_filename_B.c_str());
        return 1;
    }

    SIGNAL_INFO ref_info = {};
    SIGNAL_INFO deg_info = {};
    ERROR_INFO err_info = {};

    const size_t max_file_path = min(FILENAME_MAX, sizeof(ref_info.path_name));
    strncpy (ref_info.path_name, in_wav_A.c_str(), min(max_file_path, in_wav_A.size()));
    strncpy (deg_info.path_name, in_wav_B.c_str(), min(max_file_path, in_wav_B.size()));

    ref_info.apply_swap = 0;
    deg_info.apply_swap = 0;

    ref_info.input_filter = 2;
    deg_info.input_filter = 2;
    err_info.mode = WB_MODE;

    long err = 0;
    char error_msg[512];
    init_resamplers(16000);
    pesq_measure (&ref_info, &deg_info, &err_info, &err, (char **) &error_msg);

    if (err == 0) {
        RATE_LOGE("%.3f\n", (double) err_info.mapped_mos);
    } else {
        RATE_LOGE("P.862.2 Prediction (MOS-LQO) failed!\n");
    }
    remove(in_wav_A.c_str());
    remove(in_wav_B.c_str());

    return err;
}


