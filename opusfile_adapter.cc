#include <string>
#include <fstream>
#include <iostream>
#include "opusfile.h"
#include "opusfile_adapter.h"
#include "wav_header.h"

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

    constexpr size_t samples = 0x7FFFFFFF;
    constexpr size_t bytes_per_sample = 2;

    webrtc::WriteWavHeader((uint8_t*)header, num_channels, sample_rate, webrtc::kWavFormatPcm, bytes_per_sample, duration_in_pcm_samples);
}
} // namespace

namespace tg_rate {



bool opus_decode(const std::string& input_filename, const std::string& decoded_wav_file_name) {
    const std::string &input_file_name = input_filename;

    OggOpusFile *of;
    ogg_int64_t duration;
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

    duration = 0;
    int channels = 1;

    if (op_seekable(of)) {
        opus_int64 size;
        fprintf(stderr, "Total number of links: %i\n", op_link_count(of));
        duration = op_pcm_total(of, -1);
        fprintf(stderr, "Total duration: ");
        print_duration(stderr, duration, 3);
        fprintf(stderr, " (%li samples @ 48 kHz)\n", (long) duration);
        size = op_raw_total(of, -1);
        fprintf(stderr, "Total size: ");
        print_size(stderr, size, 0, "");


        channels = 2;// TODO (baidaev): op_channel_count(of, -1);
        fprintf(stderr, "\n");
    }

    unsigned char wav_header[webrtc::kWavHeaderSize];

    make_wav_header(wav_header, duration, 48000, channels);
    wav_file.write((const char*)wav_header, sizeof(wav_header));

    ogg_int64_t pcm_offset;
    ogg_int64_t pcm_print_offset;
    ogg_int64_t nsamples;
    opus_int32 bitrate;
    int prev_li;
    prev_li = -1;
    nsamples = 0;
    pcm_offset = op_pcm_tell(of);
    if (pcm_offset != 0) {
        fprintf(stderr, "Non-zero starting PCM offset: %li\n", (long) pcm_offset);
    }
    pcm_print_offset = pcm_offset - 48000;
    bitrate = 0;
    for (;;) {
        ogg_int64_t next_pcm_offset;
        opus_int16 pcm[120 * (48000 / 1000) * 2];
        unsigned char out[120 * (48000 / 1000) * 2 * 2];
        int li;
        int si;
        /*Although we would generally prefer to use the float interface, WAV
           files with signed, 16-bit little-endian samples are far more
           universally supported, so that's what we output.*/
        ret = op_read_stereo(of, pcm, sizeof(pcm) / sizeof(*pcm));

        if (ret == OP_HOLE) {
            fprintf(stderr, "\nHole detected! Corrupt file segment?\n");
            continue;
        } else if (ret < 0) {
            fprintf(stderr, "\nError decoding '%s': %i\n", input_file_name.c_str(), ret);
            ret = EXIT_FAILURE;
            break;
        }
        li = op_current_link(of);
        if (li != prev_li) {
            const OpusHead *head;
            int binary_suffix_len;
            int ci;
            /*We found a new link.
              Print out some information.*/
            fprintf(stderr, "Decoding link %i:                          \n", li);
            head = op_head(of, li);
            fprintf(stderr, "  Channels: %i\n", head->channel_count);
            if (op_seekable(of)) {
                ogg_int64_t duration;
                opus_int64 size;
                duration = op_pcm_total(of, li);
                fprintf(stderr, "  Duration: ");
                print_duration(stderr, duration, 3);
                fprintf(stderr, " (%li samples @ 48 kHz)\n", (long) duration);
                size = op_raw_total(of, li);
                fprintf(stderr, "  Size: ");
                print_size(stderr, size, 0, "");
                fprintf(stderr, "\n");
            }
            if (head->input_sample_rate) {
                fprintf(stderr, "  Original sampling rate: %lu Hz\n", (unsigned long) head->input_sample_rate);
            }
            if (!op_seekable(of)) {
                pcm_offset = op_pcm_tell(of) - ret;
                if (pcm_offset != 0) {
                    fprintf(stderr, "Non-zero starting PCM offset in link %i: %li\n", li, (long) pcm_offset);
                }
            }
        }
        if (li != prev_li || pcm_offset >= pcm_print_offset + 48000) {
            opus_int32 next_bitrate;
            opus_int64 raw_offset;
            next_bitrate = op_bitrate_instant(of);
            if (next_bitrate >= 0)
                bitrate = next_bitrate;
            raw_offset = op_raw_tell(of);
            fprintf(stderr, "\r ");
            print_size(stderr, raw_offset, 0, "");
            fprintf(stderr, "  ");
            print_duration(stderr, pcm_offset, 0);
            fprintf(stderr, "  (");
            print_size(stderr, bitrate, 1, " ");
            fprintf(stderr, "bps)                    \r");
            pcm_print_offset = pcm_offset;
            fflush(stderr);
        }
        next_pcm_offset = op_pcm_tell(of);
        if (pcm_offset + ret != next_pcm_offset) {
            fprintf(stderr, "PCM offset gap! %li+%i!=%li\n", (long) pcm_offset, ret, (long) next_pcm_offset);
        }
        pcm_offset = next_pcm_offset;
        if (ret <= 0) {
            ret = EXIT_SUCCESS;
            break;
        }
        /*Ensure the data is little-endian before writing it out.*/
        for (si = 0; si < 2 * ret; si++) {
            out[2 * si + 0] = (unsigned char) (pcm[si] & 0xFF);
            out[2 * si + 1] = (unsigned char) (pcm[si] >> 8 & 0xFF);
        }
        wav_file.write((const char *) out, sizeof(*out) * 4 * ret);
        if (wav_file.fail()) {
            fprintf(stderr, "Error writing decoded audio data: %s\n", strerror(errno));
            ret = EXIT_FAILURE;
            break;
        }
        nsamples += ret;
        prev_li = li;
    }
    if (ret == EXIT_SUCCESS) {
        fprintf(stderr, "Done!\n");
        print_duration(stderr, nsamples, 3);
        fprintf(stderr, " (%li samples @ 48 kHz).\n", (long) nsamples);
    }
    if (op_seekable(of) && nsamples != duration) {
        fprintf(stderr, "WARNING: Number of output samples does not match declared file duration.\n");
    }
    if (nsamples != duration) {
        make_wav_header(wav_header, nsamples, 48000, channels);

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

} // namespace tg_rate



