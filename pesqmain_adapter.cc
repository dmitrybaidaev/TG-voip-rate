#include <string>
#include <iostream>
#include "wav_file.h"
#include "opusfile.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-user-defined-literal"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/samplefmt.h>
    #include <libswresample/swresample.h>
}

#pragma clang diagnostic pop

static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt;
        const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
            { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
            { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
            { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
            { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
            { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "Sample format %s not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return AVERROR(EINVAL);
}

/**
 * Fill dst buffer with nb_samples, generated starting from t.
 */
static void fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t)
{
    int i, j;
    double tincr = 1.0 / sample_rate, *dstp = dst;
    const double c = 2 * M_PI * 440.0;

    /* generate sin tone with 440Hz frequency and duplicated channels */
    for (i = 0; i < nb_samples; i++) {
        *dstp = sin(c * *t);
        for (j = 1; j < nb_channels; j++)
            dstp[j] = dstp[0];
        dstp += nb_channels;
        *t += tincr;
    }
}

static void print_duration(FILE *_fp,ogg_int64_t _nsamples,int _frac){
    ogg_int64_t seconds;
    ogg_int64_t minutes;
    ogg_int64_t hours;
    ogg_int64_t days;
    ogg_int64_t weeks;
    _nsamples+=_frac?24:24000;
    seconds=_nsamples/48000;
    _nsamples-=seconds*48000;
    minutes=seconds/60;
    seconds-=minutes*60;
    hours=minutes/60;
    minutes-=hours*60;
    days=hours/24;
    hours-=days*24;
    weeks=days/7;
    days-=weeks*7;
    if(weeks)fprintf(_fp,"%liw",(long)weeks);
    if(weeks||days)fprintf(_fp,"%id",(int)days);
    if(weeks||days||hours){
        if(weeks||days)fprintf(_fp,"%02ih",(int)hours);
        else fprintf(_fp,"%ih",(int)hours);
    }
    if(weeks||days||hours||minutes){
        if(weeks||days||hours)fprintf(_fp,"%02im",(int)minutes);
        else fprintf(_fp,"%im",(int)minutes);
        fprintf(_fp,"%02i",(int)seconds);
    }
    else fprintf(_fp,"%i",(int)seconds);
    if(_frac)fprintf(_fp,".%03i",(int)(_nsamples/48));
    fprintf(_fp,"s");
}

static void print_size(FILE *_fp,opus_int64 _nbytes,int _metric,
                       const char *_spacer){
    static const char SUFFIXES[7]={' ','k','M','G','T','P','E'};
    opus_int64 val;
    opus_int64 den;
    opus_int64 round;
    int        base;
    int        shift;
    base=_metric?1000:1024;
    round=0;
    den=1;
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

static void put_le32(unsigned char *_dst,opus_uint32 _x){
    _dst[0]=(unsigned char)(_x&0xFF);
    _dst[1]=(unsigned char)(_x>>8&0xFF);
    _dst[2]=(unsigned char)(_x>>16&0xFF);
    _dst[3]=(unsigned char)(_x>>24&0xFF);
}

/*Make a header for a 48 kHz, stereo, signed, 16-bit little-endian PCM WAV.*/
static void make_wav_header(unsigned char _dst[44],ogg_int64_t _duration){
    /*The chunk sizes are set to 0x7FFFFFFF by default.
      Many, though not all, programs will interpret this to mean the duration is
       "undefined", and continue to read from the file so long as there is actual
       data.*/
    static const unsigned char WAV_HEADER_TEMPLATE[44]={
            'R','I','F','F',0xFF,0xFF,0xFF,0x7F,
            'W','A','V','E','f','m','t',' ',
            0x10,0x00,0x00,0x00,0x01,0x00,0x02,0x00,
            0x80,0xBB,0x00,0x00,0x00,0xEE,0x02,0x00,
            0x04,0x00,0x10,0x00,'d','a','t','a',
            0xFF,0xFF,0xFF,0x7F
    };
    memcpy(_dst,WAV_HEADER_TEMPLATE,sizeof(WAV_HEADER_TEMPLATE));
    if(_duration>0){
        if(_duration>0x1FFFFFF6){
            fprintf(stderr,"WARNING: WAV output would be larger than 2 GB.\n");
            fprintf(stderr,
                    "Writing non-standard WAV header with invalid chunk sizes.\n");
        }
        else{
            opus_uint32 audio_size;
            audio_size=(opus_uint32)(_duration*4);
            put_le32(_dst+4,audio_size+36);
            put_le32(_dst+40,audio_size);
        }
    }
}


void test_decode() {
    std::string input_file_name = "/Users/d.baydaev/work/git/Tgvoiprate/sample05_066a3936b4ebc1ca0c3b9e5d4e061e4b.ogg";

    OggOpusFile  *of;
    ogg_int64_t   duration;
    unsigned char wav_header[44];
    int           ret;
    int           output_seekable;

    FILE* wav_file = fopen("/Users/d.baydaev/work/git/Tgvoiprate/out.wav", "wb");
    if(!wav_file) {
        fprintf(stderr, "ERROR: failed to open out wav-file!\n");
        return;
    }

    of = op_open_file(input_file_name.c_str(), &ret);
    duration = 0;
    output_seekable = fseek(wav_file, 0, SEEK_CUR)!=-1;

    if(op_seekable(of)){
        opus_int64  size;
        fprintf(stderr, "Total number of links: %i\n", op_link_count(of));
        duration = op_pcm_total(of, -1);
        fprintf(stderr, "Total duration: ");
        print_duration(stderr, duration, 3);
        fprintf(stderr," (%li samples @ 48 kHz)\n",(long)duration);
        size = op_raw_total(of, -1);
        fprintf(stderr, "Total size: ");
        print_size(stderr, size, 0, "");
        fprintf(stderr, "\n");
    } else if(!output_seekable){
        fprintf(stderr,"WARNING: Neither input nor output are seekable.\n");
        fprintf(stderr,
                "Writing non-standard WAV header with invalid chunk sizes.\n");
    }

    make_wav_header(wav_header,duration);

    if(!fwrite(wav_header, sizeof(wav_header), 1, wav_file)){
        fprintf(stderr,"Error writing WAV header: %s\n",strerror(errno));
        goto done;
    }

    ogg_int64_t pcm_offset;
    ogg_int64_t pcm_print_offset;
    ogg_int64_t nsamples;
    opus_int32  bitrate;
    int         prev_li;
    prev_li=-1;
    nsamples=0;
    pcm_offset=op_pcm_tell(of);
    if(pcm_offset!=0){
        fprintf(stderr,"Non-zero starting PCM offset: %li\n",(long)pcm_offset);
    }
    pcm_print_offset=pcm_offset-48000;
    bitrate=0;
    for(;;){
        ogg_int64_t   next_pcm_offset;
        opus_int16    pcm[120*48*2];
        unsigned char out[120*48*2*2];
        int           li;
        int           si;
        /*Although we would generally prefer to use the float interface, WAV
           files with signed, 16-bit little-endian samples are far more
           universally supported, so that's what we output.*/
        ret = op_read_stereo(of,pcm,sizeof(pcm)/sizeof(*pcm));

        if(ret == OP_HOLE){
            fprintf(stderr,"\nHole detected! Corrupt file segment?\n");
            continue;
        }
        else if(ret<0){
            fprintf(stderr,"\nError decoding '%s': %i\n", input_file_name.c_str(), ret);
            ret = EXIT_FAILURE;
            break;
        }
        li = op_current_link(of);
        if(li != prev_li){
            const OpusHead *head;
            const OpusTags *tags;
            int             binary_suffix_len;
            int             ci;
            /*We found a new link.
              Print out some information.*/
            fprintf(stderr,"Decoding link %i:                          \n",li);
            head=op_head(of,li);
            fprintf(stderr,"  Channels: %i\n",head->channel_count);
            if(op_seekable(of)){
                ogg_int64_t duration;
                opus_int64  size;
                duration=op_pcm_total(of,li);
                fprintf(stderr,"  Duration: ");
                print_duration(stderr,duration,3);
                fprintf(stderr," (%li samples @ 48 kHz)\n",(long)duration);
                size=op_raw_total(of,li);
                fprintf(stderr,"  Size: ");
                print_size(stderr,size,0,"");
                fprintf(stderr,"\n");
            }
            if(head->input_sample_rate){
                fprintf(stderr,"  Original sampling rate: %lu Hz\n",
                        (unsigned long)head->input_sample_rate);
            }
            tags=op_tags(of,li);
            fprintf(stderr,"  Encoded by: %s\n",tags->vendor);
            for(ci=0;ci<tags->comments;ci++){
                const char *comment;
                comment=tags->user_comments[ci];
                if(opus_tagncompare("METADATA_BLOCK_PICTURE",22,comment)==0){
                    OpusPictureTag pic;
                    int            err;
                    err=opus_picture_tag_parse(&pic,comment);
                    fprintf(stderr,"  %.23s",comment);
                    if(err>=0){
                        fprintf(stderr,"%u|%s|%s|%ux%ux%u",pic.type,pic.mime_type,
                                pic.description,pic.width,pic.height,pic.depth);
                        if(pic.colors!=0)fprintf(stderr,"/%u",pic.colors);
                        if(pic.format==OP_PIC_FORMAT_URL){
                            fprintf(stderr,"|%s\n",pic.data);
                        }
                        else{
                            fprintf(stderr,"|<%u bytes of image data>\n",pic.data_length);
                        }
                        opus_picture_tag_clear(&pic);
                    }
                    else fprintf(stderr,"<error parsing picture tag>\n");
                }
                else fprintf(stderr,"  %s\n",tags->user_comments[ci]);
            }
            if(opus_tags_get_binary_suffix(tags,&binary_suffix_len)!=NULL){
                fprintf(stderr,"<%u bytes of unknown binary metadata>\n",
                        binary_suffix_len);
            }
            fprintf(stderr,"\n");
            if(!op_seekable(of)){
                pcm_offset=op_pcm_tell(of)-ret;
                if(pcm_offset!=0){
                    fprintf(stderr,"Non-zero starting PCM offset in link %i: %li\n",
                            li,(long)pcm_offset);
                }
            }
        }
        if(li != prev_li || pcm_offset >= pcm_print_offset + 48000){
            opus_int32 next_bitrate;
            opus_int64 raw_offset;
            next_bitrate = op_bitrate_instant(of);
            if(next_bitrate >= 0) bitrate = next_bitrate;
            raw_offset = op_raw_tell(of);
            fprintf(stderr,"\r ");
            print_size(stderr,raw_offset, 0, "");
            fprintf(stderr,"  ");
            print_duration(stderr, pcm_offset, 0);
            fprintf(stderr,"  (");
            print_size(stderr, bitrate, 1, " ");
            fprintf(stderr,"bps)                    \r");
            pcm_print_offset = pcm_offset;
            fflush(stderr);
        }
        next_pcm_offset = op_pcm_tell(of);
        if(pcm_offset + ret != next_pcm_offset){
            fprintf(stderr,"\nPCM offset gap! %li+%i!=%li\n",
                    (long)pcm_offset,ret,(long)next_pcm_offset);
        }
        pcm_offset=next_pcm_offset;
        if(ret<=0){
            ret=EXIT_SUCCESS;
            break;
        }
        /*Ensure the data is little-endian before writing it out.*/
        for(si=0;si<2*ret;si++){
            out[2*si+0]=(unsigned char)(pcm[si]&0xFF);
            out[2*si+1]=(unsigned char)(pcm[si]>>8&0xFF);
        }
        if(!fwrite(out, sizeof(*out)*4*ret, 1, wav_file)){
            fprintf(stderr,"\nError writing decoded audio data: %s\n",
                    strerror(errno));
            ret=EXIT_FAILURE;
            break;
        }
        nsamples+=ret;
        prev_li=li;
    }
    if(ret==EXIT_SUCCESS){
        fprintf(stderr,"\nDone: played ");
        print_duration(stderr,nsamples,3);
        fprintf(stderr," (%li samples @ 48 kHz).\n",(long)nsamples);
    }
    if(op_seekable(of)&&nsamples!=duration){
        fprintf(stderr,"\nWARNING: "
                       "Number of output samples does not match declared file duration.\n");
        if(!output_seekable)fprintf(stderr,"Output WAV file will be corrupt.\n");
    }
    if(output_seekable&&nsamples != duration){
        make_wav_header(wav_header, nsamples);
        if(fseek(wav_file, 0, SEEK_SET)||
           !fwrite(wav_header, sizeof(wav_header), 1, wav_file)){
            fprintf(stderr,"Error rewriting WAV header: %s\n",strerror(errno));
            ret=EXIT_FAILURE;
        }
    }
    op_free(of);

done:
    if(wav_file) {
        fflush(wav_file);
        fclose(wav_file);
    }
    wav_file = nullptr;
}

int main(int argc, char **argv)
{
    test_decode();
    std::string in_file, out_file;

    int64_t src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_SURROUND;
    int src_rate = 48000, dst_rate;
    uint8_t **src_data = NULL, **dst_data = NULL;
    int src_nb_channels = 0, dst_nb_channels = 0;
    int src_linesize, dst_linesize;
    int src_nb_samples = 1024, dst_nb_samples, max_dst_nb_samples;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_DBL, dst_sample_fmt = AV_SAMPLE_FMT_S16;
    const char *dst_filename = NULL;
    FILE *dst_file;
    int dst_bufsize;
    const char *fmt;
    struct SwrContext *swr_ctx;
    double t;
    int ret;

    if (argc != 4) {
        printf("Usage: %s <in_file> <out_file> <out_sample_rate>\n",  argv[0]);
        return 1;
    }
    dst_filename = argv[1];
    out_file = argv[2];

    dst_rate = atoi(argv[3]);
    webrtc::WavReader wav_reader(argv[1]);
    std::cout << "reading file: " << in_file << ", SR:" << wav_reader.sample_rate()
              << ", ch:" << wav_reader.num_channels() << ", samples:" << wav_reader.num_samples();

    dst_file = fopen(out_file.c_str(), "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }

    /* create resampler context */
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* set options */
    av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        goto end;
    }

    /* allocate source and destination samples buffers */

    src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
                                             src_nb_samples, src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto end;
    }

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    max_dst_nb_samples = dst_nb_samples =
            av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

    /* buffer is going to be directly written to a rawaudio file, no alignment */
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                             dst_nb_samples, dst_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        goto end;
    }

    t = 0;
    do {
        /* generate synthetic audio */
        fill_samples((double *)src_data[0], src_nb_samples, src_nb_channels, src_rate, &t);

        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
                                        src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
        if (dst_nb_samples > max_dst_nb_samples) {
            av_freep(&dst_data[0]);
            ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
                                   dst_nb_samples, dst_sample_fmt, 1);
            if (ret < 0)
                break;
            max_dst_nb_samples = dst_nb_samples;
        }

        /* convert to destination format */
        ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            goto end;
        }
        dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                 ret, dst_sample_fmt, 1);
        if (dst_bufsize < 0) {
            fprintf(stderr, "Could not get sample buffer size\n");
            goto end;
        }
        printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
        fwrite(dst_data[0], 1, dst_bufsize, dst_file);
    } while (t < 10);

    if ((ret = get_format_from_sample_fmt(&fmt, dst_sample_fmt)) < 0)
        goto end;
//    fprintf(stderr, "Resampling succeeded. Play the output file with the command:\n"
//                    "ffplay -f %s -channel_layout %"PRId64" -channels %d -ar %d %s\n",
//            fmt, dst_ch_layout, dst_nb_channels, dst_rate, dst_filename);

    end:
    fclose(dst_file);

    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);

    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);

    swr_free(&swr_ctx);
    return ret < 0;
}


