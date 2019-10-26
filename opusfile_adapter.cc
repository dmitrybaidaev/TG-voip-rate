#include <string>
#include <fstream>
#include <iostream>
#include "opusfile.h"
#include "opusfile_adapter.h"
#include "wav_header.h"
#include "wav_file.h"
#include "resampler.h"
#include "rate_log.h"

namespace {
void print_duration(ogg_int64_t _nsamples,int _frac){
    ogg_int64_t seconds;
    ogg_int64_t minutes;
    ogg_int64_t hours;
    ogg_int64_t days;
    ogg_int64_t weeks;
    _nsamples +=_frac?24:24000;
    seconds = _nsamples/48000;
    _nsamples -= seconds*48000;
    minutes = seconds/60;
    seconds -= minutes*60;
    hours = minutes/60;
    minutes -= hours*60;
    days = hours/24;
    hours -= days*24;
    weeks = days/7;
    days -= weeks*7;
    
    if(weeks)
        RATE_LOGI("%liw",(long)weeks);
    if(weeks || days)
        RATE_LOGI("%id",(int)days);
    
    if(weeks || days || hours){
        if(weeks||days) {
            RATE_LOGI("%02ih",(int)hours);
        } else {
            RATE_LOGI("%ih", (int) hours);
        }
    }

    if(weeks || days || hours || minutes){
        if(weeks || days || hours) {
            RATE_LOGI("%02im",(int)minutes);
        } else {
            RATE_LOGI("%im",(int)minutes);
        }
        RATE_LOGI("%02i",(int)seconds);
    } else
        RATE_LOGI("%i",(int)seconds);
    if(_frac) {
        RATE_LOGI(".%03i", (int)(_nsamples/48));
    }
    RATE_LOGI("s\n");
}

void print_size(FILE *_fp,opus_int64 _nbytes,int _metric, const char *_spacer) {
    static const char SUFFIXES[7] = {' ','k','M','G','T','P','E'};
    opus_int64 val;
    opus_int64 den;
    opus_int64 round;
    int        base;
    int        shift;
    base = _metric?1000:1024;
    round = 0;
    den = 1;
    for(shift=0;shift<6;shift++){
        if(_nbytes<den*base-round)break;
        den*=base;
        round=den>>1;
    }
    val=(_nbytes+round)/den;
    if(den>1&&val<10){
        if(den>=1000000000)val=(_nbytes+(round/100))/(den/100);
        else val=(_nbytes*100+round)/den;
        RATE_LOGI("%li.%02i%s%c",(long)(val/100),(int)(val%100),
                _spacer,SUFFIXES[shift]);
    }
    else if(den>1&&val<100){
        if(den>=1000000000)val=(_nbytes+(round/10))/(den/10);
        else val=(_nbytes*10+round)/den;
        RATE_LOGI("%li.%i%s%c",(long)(val/10),(int)(val%10),
                _spacer,SUFFIXES[shift]);
    }
    else RATE_LOGI("%li%s%c",(long)val,_spacer,SUFFIXES[shift]);
}

/*Make a header for a 48 kHz, mono, signed, 16-bit little-endian PCM WAV.*/
void make_wav_header(unsigned char header[webrtc::kWavHeaderSize], ogg_int64_t duration_in_pcm_samples, size_t sample_rate, size_t num_channels) {
    /*The chunk sizes are set to 0x7FFFFFFF by default.
      Many, though not all, programs will interpret this to mean the duration is
       "undefined", and continue to read from the file so long as there is actual
       data.*/

    constexpr size_t bytes_per_sample = 2;

    webrtc::WriteWavHeader((uint8_t*)header, num_channels, sample_rate, webrtc::kWavFormatPcm, bytes_per_sample, duration_in_pcm_samples);
}
} // namespace

namespace tg_rate {



bool opus_decode(const std::string& input_filename, const std::string& decoded_wav_file_name) {
    const std::string &input_file_name = input_filename;

    OggOpusFile *of;
    ogg_int64_t duration_in_samples;
    int ret;

    std::ofstream wav_file(decoded_wav_file_name, std::ios::out | std::ios::binary);
    if (!wav_file.is_open()) {
        RATE_LOGE("ERROR: failed to open output wav-file (%s)!\n", decoded_wav_file_name.c_str());
        return false;
    }

    of = op_open_file(input_file_name.c_str(), &ret);
    if (of == nullptr) {
        RATE_LOGE("ERROR: failed to open input opus(ogg container)-file (%s)!\n", input_filename.c_str());
        return false;
    }

    duration_in_samples = 0;
    int channels = 2;

    if (op_seekable(of)) {
        opus_int64 size;
        RATE_LOGI("Total number of links: %i\n", op_link_count(of));
        duration_in_samples = op_pcm_total(of, -1);
        RATE_LOGI("Total duration: ");
        print_duration(duration_in_samples, 3);
        RATE_LOGI(" (%li samples @ 48 kHz)\n", (long) duration_in_samples);
        size = op_raw_total(of, -1);
        RATE_LOGI("Total size: ");
        print_size(stdout, size, 0, "");

        channels = 2;// TODO (baidaev): op_channel_count(of, -1);
        RATE_LOGI("\n");
    }

    unsigned char wav_header[webrtc::kWavHeaderSize];

    make_wav_header(wav_header, duration_in_samples, 48000, channels);
    wav_file.write((const char*)wav_header, sizeof(wav_header));

    ogg_int64_t total_samples_read = 0;
    int prev_li = -1;
    ogg_int64_t pcm_offset = op_pcm_tell(of);
    if (pcm_offset != 0) {
        RATE_LOGE("Non-zero starting PCM offset: %li\n", (long) pcm_offset);
    }

    static_assert(sizeof(opus_int16) == 2 * sizeof(opus_uint8), "invalid size assumptions!");
    static_assert(sizeof(opus_uint8) == 1, "invalid size assumptions!");

    constexpr size_t kStereoFrameSize = 120 * 48 * 2;
    constexpr size_t kSampleSize = sizeof(opus_int16);

    for (;;) {
        opus_int16 pcm[kStereoFrameSize] = {0};
        opus_uint8 out[kStereoFrameSize * 2] = {0};

        /*Although we would generally prefer to use the float interface, WAV
           files with signed, 16-bit little-endian samples are far more
           universally supported, so that's what we output.*/
        const int samples_read = op_read_stereo(of, pcm, sizeof(pcm) / sizeof(*pcm));

        if (samples_read == OP_HOLE) {
            RATE_LOGE("\nHole detected! Corrupt file segment?\n");
            continue;
        } else if (samples_read < 0) {
            RATE_LOGE("\nError decoding '%s': %i\n", input_file_name.c_str(), ret);
            ret = EXIT_FAILURE;
            break;
        }
        int li = op_current_link(of);
        if (li != prev_li) {
            const OpusHead *head;
            /*We found a new link.
              Print out some information.*/
            RATE_LOGI("Decoding link: %i                          \n", li);
            head = op_head(of, li);
            RATE_LOGI("  Channels: %i\n", head->channel_count);

            if (head->input_sample_rate) {
                RATE_LOGI("  Original sampling rate: %lu Hz\n", (unsigned long) head->input_sample_rate);
            }
            if (!op_seekable(of)) {
                pcm_offset = op_pcm_tell(of) - samples_read;
                if (pcm_offset != 0) {
                    RATE_LOGI("Non-zero starting PCM offset in link %i: %li\n", li, (long) pcm_offset);
                }
            }
        }
        ogg_int64_t  next_pcm_offset = op_pcm_tell(of);
        if (pcm_offset + samples_read != next_pcm_offset) {
            RATE_LOGI("PCM offset gap! %li+%i!=%li\n", (long) pcm_offset, ret, (long) next_pcm_offset);
        }
        pcm_offset = next_pcm_offset;
        if (samples_read <= 0) {
            ret = EXIT_SUCCESS;
            break;
        }
        /*Ensure the data is little-endian before writing it out.*/
        for (size_t i = 0; i < samples_read * kSampleSize; i++) {
            out[2 * i + 0] = (unsigned char) ( (pcm[i]     ) & 0xFF);
            out[2 * i + 1] = (unsigned char) ( (pcm[i] >> 8) & 0xFF);
        }
        wav_file.write((const char *) out, 2 * kSampleSize * samples_read);
        if (wav_file.fail()) {
            RATE_LOGE("Error writing decoded audio data!\n");
            ret = EXIT_FAILURE;
            break;
        }
        total_samples_read += samples_read;
        prev_li = li;
    }
    if (ret == EXIT_SUCCESS) {
        RATE_LOGI("Done!\n");
        print_duration(total_samples_read, 3);
        RATE_LOGI(" (%li samples @ 48 kHz).\n", (long) total_samples_read);
    }
    if (op_seekable(of) && total_samples_read != duration_in_samples) {
        RATE_LOGI("WARNING: Number of output samples does not match declared file duration_in_samples.\n");
    }
    if (total_samples_read != duration_in_samples) {
        make_wav_header(wav_header, total_samples_read, 48000, channels);

        wav_file.seekp(std::ios_base::beg);
        wav_file << wav_header;
        if (wav_file.fail()) {
            RATE_LOGE("Error rewriting WAV header!\n");
            ret = EXIT_FAILURE;
        }
    }
    op_free(of);

    return ret == EXIT_SUCCESS;
}

bool opus_decode_mono_16khz(const std::string& input_filename, const std::string& decoded_16khz_name) {
    const std::string& decoded_filename_tmp = input_filename + "_dec_48khz_tmp.wav";

    if(!tg_rate::opus_decode(input_filename, decoded_filename_tmp)) {
        RATE_LOGE("failed to decode Opus file: %s\n", decoded_filename_tmp.c_str());
        return 1;
    }

    if(!tg_rate::resample(decoded_filename_tmp, decoded_16khz_name, 16000, 1)) {
        RATE_LOGE("failed to resample Wav file: %s -> mono @ 16kHz\n", decoded_16khz_name.c_str());
        return 1;
    }

    remove(decoded_filename_tmp.c_str());
    return true;
}

bool opus_encode(const std::string& input_wav_filename, const std::string& encoded_file_name) {
    webrtc::WavReader wav_reader(input_wav_filename);
    const auto src_nb_samples = wav_reader.num_samples();
    const auto in_sample_rate = wav_reader.sample_rate();
    const auto in_channels = wav_reader.num_channels();

    RATE_LOGI("Info: reading file: %s, sr:%d, ch:%d, samples:%d\n",
              input_wav_filename.c_str(), in_sample_rate, in_channels, src_nb_samples);

    if(in_sample_rate == 0 || wav_reader.num_channels() == 0 || src_nb_samples == 0) {
        RATE_LOGE("Error: failed to read input wav-file (%s)\n", input_wav_filename.c_str());
        return false;
    }

    constexpr size_t FRAME_SIZE = 480;
    constexpr size_t MAX_FRAME_SIZE = 6 * FRAME_SIZE;
    constexpr size_t SAMPLE_RATE = 48000;
    constexpr size_t CHANNELS = 1;
    constexpr size_t BITRATE = 64000;
    constexpr size_t MAX_PACKET_SIZE = 3 * 1276;

#define CHECK_ERR(err, text) if((err) < 0) { RATE_LOGE(text); break;}
    OpusEncoder *encoder = nullptr;
    int err = 0;

    opus_int16 in[FRAME_SIZE * CHANNELS] = {};
    unsigned char cbits[MAX_PACKET_SIZE] = {0};

    do {
        std::ofstream encoded_file(encoded_file_name, std::ios::out | std::ios::binary);
        if(!encoded_file.good()) {
            RATE_LOGE("failed to open output file, %s\n", encoded_file_name.c_str())
        }
        encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &err);
        CHECK_ERR(err, "Failed to create encode!\n");

        err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
        CHECK_ERR(err, "failed to set bitrate\n");

        size_t bytes = 0;
        for(;;) {
            uint8_t pcm_bytes[MAX_FRAME_SIZE * CHANNELS * 2];

            size_t samples_read = wav_reader.ReadSamples(FRAME_SIZE, (int16_t*) pcm_bytes);
            /* Read a 16 bits/sample audio frame. */
            if (samples_read == 0)
                break;

            /* Convert from little-endian ordering. */
            for (size_t i = 0; i < CHANNELS * FRAME_SIZE; i++) {
                in[i] = (pcm_bytes[2 * i + 1] << 8) | (pcm_bytes[ 2 * i] << 0);
            }

            /* Encode the frame. */
            int bytes_encoded = opus_encode(encoder, in, FRAME_SIZE, cbits, MAX_PACKET_SIZE);
            if (bytes_encoded < 0) {
                RATE_LOGE("encode failed: %s\n", opus_strerror(bytes_encoded));
                return EXIT_FAILURE;
            }
            bytes += bytes_encoded;

            encoded_file.write((const char*)cbits, bytes_encoded);
        }
        RATE_LOGI("Done! bytes encoded:%d\n", bytes);

    } while(false);
    opus_encoder_destroy(encoder);

    return true;
}

} // namespace tg_rate



