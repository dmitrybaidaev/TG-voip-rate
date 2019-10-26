#include <string>
#include <fstream>
#include <iostream>
#include "opusfile.h"
#include "opusfile_adapter.h"
#include "wav_header.h"
#include "resampler.h"

namespace {
void print_duration(FILE *_fp,ogg_int64_t _nsamples,int _frac){
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
        fprintf(_fp,"%liw",(long)weeks);
    if(weeks || days)
        fprintf(_fp,"%id",(int)days);
    
    if(weeks || days || hours){
        if(weeks||days)
            fprintf(_fp,"%02ih",(int)hours);
        else 
            fprintf(_fp,"%ih",(int)hours);
    }

    if(weeks || days || hours || minutes){
        if(weeks || days || hours)
            fprintf(_fp,"%02im",(int)minutes);
        else 
            fprintf(_fp,"%im",(int)minutes);
        fprintf(_fp,"%02i",(int)seconds);
    } else 
        fprintf(_fp,"%i",(int)seconds);
    if(_frac)
        fprintf(_fp, ".%03i", (int)(_nsamples/48));
    fprintf(_fp, "s");
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
        fprintf(_fp,"%li.%02i%s%c",(long)(val/100),(int)(val%100),
                _spacer,SUFFIXES[shift]);
    }
    else if(den>1&&val<100){
        if(den>=1000000000)val=(_nbytes+(round/10))/(den/10);
        else val=(_nbytes*10+round)/den;
        fprintf(_fp,"%li.%i%s%c",(long)(val/10),(int)(val%10),
                _spacer,SUFFIXES[shift]);
    }
    else fprintf(_fp,"%li%s%c",(long)val,_spacer,SUFFIXES[shift]);
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
        fprintf(stderr, "ERROR: failed to open output wav-file (%s)!\n", decoded_wav_file_name.c_str());
        return false;
    }

    of = op_open_file(input_file_name.c_str(), &ret);
    if (of == nullptr) {
        fprintf(stderr, "ERROR: failed to open input opus(ogg container)-file (%s)!\n", input_filename.c_str());
        return false;
    }

    duration_in_samples = 0;
    int channels = 2;

    if (op_seekable(of)) {
        opus_int64 size;
        fprintf(stderr, "Total number of links: %i\n", op_link_count(of));
        duration_in_samples = op_pcm_total(of, -1);
        fprintf(stderr, "Total duration: ");
        print_duration(stderr, duration_in_samples, 3);
        fprintf(stderr, " (%li samples @ 48 kHz)\n", (long) duration_in_samples);
        size = op_raw_total(of, -1);
        fprintf(stderr, "Total size: ");
        print_size(stderr, size, 0, "");

        channels = 2;// TODO (baidaev): op_channel_count(of, -1);
        fprintf(stderr, "\n");
    }

    unsigned char wav_header[webrtc::kWavHeaderSize];

    make_wav_header(wav_header, duration_in_samples, 48000, channels);
    wav_file.write((const char*)wav_header, sizeof(wav_header));

    ogg_int64_t total_samples_read = 0;
    int prev_li = -1;
    ogg_int64_t pcm_offset = op_pcm_tell(of);
    if (pcm_offset != 0) {
        fprintf(stderr, "Non-zero starting PCM offset: %li\n", (long) pcm_offset);
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
            fprintf(stderr, "\nHole detected! Corrupt file segment?\n");
            continue;
        } else if (samples_read < 0) {
            fprintf(stderr, "\nError decoding '%s': %i\n", input_file_name.c_str(), ret);
            ret = EXIT_FAILURE;
            break;
        }
        int li = op_current_link(of);
        if (li != prev_li) {
            const OpusHead *head;
            /*We found a new link.
              Print out some information.*/
            fprintf(stderr, "Decoding link: %i                          \n", li);
            head = op_head(of, li);
            fprintf(stderr, "  Channels: %i\n", head->channel_count);

            if (head->input_sample_rate) {
                fprintf(stderr, "  Original sampling rate: %lu Hz\n", (unsigned long) head->input_sample_rate);
            }
            if (!op_seekable(of)) {
                pcm_offset = op_pcm_tell(of) - samples_read;
                if (pcm_offset != 0) {
                    fprintf(stderr, "Non-zero starting PCM offset in link %i: %li\n", li, (long) pcm_offset);
                }
            }
        }
        ogg_int64_t  next_pcm_offset = op_pcm_tell(of);
        if (pcm_offset + samples_read != next_pcm_offset) {
            fprintf(stderr, "PCM offset gap! %li+%i!=%li\n", (long) pcm_offset, ret, (long) next_pcm_offset);
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
            fprintf(stderr, "Error writing decoded audio data: %s\n", strerror(errno));
            ret = EXIT_FAILURE;
            break;
        }
        total_samples_read += samples_read;
        prev_li = li;
    }
    if (ret == EXIT_SUCCESS) {
        fprintf(stderr, "Done!\n");
        print_duration(stderr, total_samples_read, 3);
        fprintf(stderr, " (%li samples @ 48 kHz).\n", (long) total_samples_read);
    }
    if (op_seekable(of) && total_samples_read != duration_in_samples) {
        fprintf(stderr, "WARNING: Number of output samples does not match declared file duration_in_samples.\n");
    }
    if (total_samples_read != duration_in_samples) {
        make_wav_header(wav_header, total_samples_read, 48000, channels);

        wav_file.seekp(std::ios_base::beg);
        wav_file << wav_header;
        if (wav_file.fail()) {
            fprintf(stderr, "Error rewriting WAV header: %s\n", strerror(errno));
            ret = EXIT_FAILURE;
        }
    }
    op_free(of);

    return ret == EXIT_SUCCESS;
}

bool opus_decode_mono_16khz(const std::string& input_filename, const std::string& decoded_16khz_name) {
    const std::string& decoded_filename_tmp = input_filename + "_dec_48khz_tmp.wav";

    if(!tg_rate::opus_decode(input_filename, decoded_filename_tmp)) {
        fprintf(stderr, "failed to decode Opus file: %s\n", decoded_filename_tmp.c_str());
        return 1;
    }

    if(!tg_rate::resample(decoded_filename_tmp, decoded_16khz_name, 16000, 1)) {
        fprintf(stderr, "failed to resample Wav file: %s -> mono @ 16kHz\n", decoded_16khz_name.c_str());
        return 1;
    }

    remove(decoded_filename_tmp.c_str());
    return true;
}

} // namespace tg_rate



