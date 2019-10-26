#include <string>
#include <iostream>
#include <fstream>
#include "wav_file.h"
#include "wav_header.h"

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

namespace tg_rate {

bool resample(const std::string& in_file, const std::string& out_file, const size_t out_sample_rate, const size_t out_channels) {
    uint8_t **src_data = nullptr, **dst_data = nullptr;
    enum AVSampleFormat in_sample_fmt = AV_SAMPLE_FMT_S16, dst_sample_fmt = AV_SAMPLE_FMT_S16;

    struct SwrContext *swr_ctx;
    int ret;

    webrtc::WavReader wav_reader(in_file);
    const auto src_nb_samples = wav_reader.num_samples();
    const auto in_sample_rate = wav_reader.sample_rate();
    const auto in_ch_layout = wav_reader.num_channels() == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    const auto out_ch_layout = AV_CH_LAYOUT_MONO;

    std::cout << "Info: reading file: " << in_file << ", SR:" << in_sample_rate
              << ", ch:" << wav_reader.num_channels() << ", samples:" << src_nb_samples << std::endl;

    if(in_sample_rate == 0 || wav_reader.num_channels() == 0 || src_nb_samples == 0) {
        fprintf(stderr, "Error: failed to read input wav-file (%s)\n", in_file.c_str());
        return false;
    }

    bool success = false;
    do {
        /* create resampler context */
        swr_ctx = swr_alloc();
        if (!swr_ctx) {
            fprintf(stderr, "Could not allocate resampler context\n");
            break;
        }

        /* set options */
        av_opt_set_int(swr_ctx, "in_channel_layout",    in_ch_layout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate",       in_sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", in_sample_fmt, 0);

        av_opt_set_int(swr_ctx, "out_channel_layout",    out_ch_layout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate",       out_sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

        /* initialize the resampling context */
        if ((ret = swr_init(swr_ctx)) < 0) {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            break;
        }

        /* allocate source and destination samples buffers */
        int src_linesize = 0, dst_linesize = 0;
        const auto src_nb_channels = av_get_channel_layout_nb_channels(in_ch_layout);
        ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels, src_nb_samples, in_sample_fmt, 0);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate source samples\n");
            break;
        }

        /* compute the number of converted samples: buffering is avoided
         * ensuring that the output buffer will contain at least all the
         * converted input samples */
        const int dst_nb_samples = av_rescale_rnd(src_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);

        /* buffer is going to be directly written to a rawaudio file, no alignment */
        auto dst_nb_channels = av_get_channel_layout_nb_channels(out_ch_layout);
        ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels, dst_nb_samples, dst_sample_fmt, 0);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate destination samples\n");
            break;
        }

        size_t samples_read = wav_reader.ReadSamples(src_nb_samples, (int16_t*) *src_data);
        if(samples_read != src_nb_samples || samples_read == 0) {
            fprintf(stdout, "Error: failed to read wav-file, samples requested:%lu, actually read:%lu\n", src_nb_samples, samples_read);
            break;
        }

        /* convert to destination format */
        ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            break;
        }

        if (av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, ret, dst_sample_fmt, 1) < 0) {
            break;
        }
#if 1
        std::ofstream wav_file(out_file, std::ios::out | std::ios::binary);
        if(!wav_file.good()) {
            fprintf(stderr, "Error: failed to open out file: %s\n", out_file.c_str());
            break;
        }

        constexpr size_t bytes_per_sample = 2;
        unsigned char wav_header[webrtc::kWavHeaderSize];
        webrtc::WriteWavHeader(wav_header, out_channels, out_sample_rate, webrtc::kWavFormatPcm, bytes_per_sample, dst_nb_samples);

        wav_file.write((const char*)wav_header, sizeof(wav_header));
        wav_file.write((const char *)*dst_data, bytes_per_sample * dst_nb_samples);
        if (wav_file.fail()) {
            fprintf(stderr, "Error: failed to write output wav-file\n");
            break;
        }
#else
        webrtc::WavWriter wav_writer(out_filename, out_sample_rate, out_channels);
        if(!file_exists(out_filename)) {
            fprintf(stderr, "Error: failed to open out file: %s\n", out_filename.c_str());
            break;
        }
        wav_writer.WriteSamples((const int16_t*)dst_data, dst_nb_samples);
#endif
        success = true;

        fprintf(stdout, "Info: Resampling done: %s -> %s\n input: sr:%d, ch:%zu out sr:%zu, ch:%zu, total samples:%d\n",
                in_file.c_str(), out_file.c_str(), in_sample_rate, wav_reader.num_channels(),
                out_sample_rate, out_channels, dst_nb_samples);
    } while(false);

    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);

    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);

    swr_free(&swr_ctx);
    return success;
}


} // namespace tg_rate
