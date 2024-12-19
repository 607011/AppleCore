#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <regex>
#include <string>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "defs.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

std::string glob_to_regex(std::string const& glob)
{
    std::string regex;
    regex.reserve(glob.size() << 1);
    for (auto c : glob)
    {
        switch (c)
        {
        case '*':
            regex += ".*";
            break;
        case '?':
            regex += '.';
            break;
        case '.':
            regex += "\\.";
            break;
        default:
            regex += c;
            break;
        }
    }
    return regex;
}

inline uint8_t RGB2Y(uint8_t r, uint8_t g, uint8_t b)
{
    return ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
}

inline uint8_t RGB2U(uint8_t r, uint8_t g, uint8_t b)
{
    return ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
}

inline uint8_t RGB2V(uint8_t r, uint8_t g, uint8_t b)
{
    return ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
}

void RGBAtoYUV420P(const uint8_t* rgba, uint8_t* y, uint8_t* u, uint8_t* v, int width, int height, int y_stride,
                   int uv_stride)
{
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            const uint8_t* pixel = rgba + (j * width + i) * 4;
            y[j * y_stride + i] = RGB2Y(pixel[0], pixel[1], pixel[2]);
        }
    }

    // UV planes are quarter size, so we sample every 2x2 pixels
    for (int j = 0; j < height; j += 2)
    {
        for (int i = 0; i < width; i += 2)
        {
            const uint8_t* pixel = rgba + (j * width + i) * 4;

            // Average 2x2 block of pixels
            int r = 0, g = 0, b = 0;

            // Top-left pixel
            r += pixel[0];
            g += pixel[1];
            b += pixel[2];

            // Top-right pixel
            if (i + 1 < width)
            {
                const uint8_t* p = rgba + (j * width + i + 1) * 4;
                r += p[0];
                g += p[1];
                b += p[2];
            }

            // Bottom-left pixel
            if (j + 1 < height)
            {
                const uint8_t* p = rgba + ((j + 1) * width + i) * 4;
                r += p[0];
                g += p[1];
                b += p[2];
            }

            // Bottom-right pixel
            if (i + 1 < width && j + 1 < height)
            {
                const uint8_t* p = rgba + ((j + 1) * width + i + 1) * 4;
                r += p[0];
                g += p[1];
                b += p[2];
            }

            // Average the values
            r /= 4;
            g /= 4;
            b /= 4;

            // Write UV values
            int uv_index = (j / 2) * uv_stride + (i / 2);
            u[uv_index] = RGB2U(r, g, b);
            v[uv_index] = RGB2V(r, g, b);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
        return 1;
    fs::path pattern{argv[1]};
    std::regex re{glob_to_regex(pattern.string())};
    std::vector<fs::directory_entry> filtered_files;
    for (const auto& entry : fs::directory_iterator(pattern.parent_path()))
    {
        if (fs::is_regular_file(entry) && std::regex_match(entry.path().string(), re))
        {
            filtered_files.push_back(entry);
        }
    }
    std::sort(std::begin(filtered_files), std::end(filtered_files));

    int width;
    int height;
    iteration_count_t max_iterations;
    fs::path const& path = filtered_files.front().path();
    std::vector<iteration_count_t> const& data = load_result(path.string(), width, height, max_iterations);

    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_alloc_output_context2(&fmt_ctx, nullptr, "mp4", "output.mp4");
    if (ret < 0)
    {
        std::cerr << "avformat_alloc_output_context2() failed.\n";
        return 1;
    }

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        std::cerr << "avcodec_find_encoder() failed.\n";
        return 1;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        std::cerr << "avcodec_alloc_context3() failed.\n";
        return 1;
    }

    codec_ctx->bit_rate = 750000LL;
    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->gop_size = 12;
    codec_ctx->max_b_frames = 2;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    ret = av_opt_set(codec_ctx->priv_data, "crf", "22", 0);
    if (ret < 0)
    {
        std::cerr << "av_opt_set() failed.\n";
        return ret;
    }
    ret = av_opt_set(codec_ctx->priv_data, "preset", "slow", 0);
    if (ret < 0)
    {
        std::cerr << "av_opt_set() failed.\n";
        return ret;
    }
    ret = av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
    if (ret < 0)
    {
        std::cerr << "av_opt_set() failed.\n";
        return ret;
    }

    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0)
    {
        std::cerr << "avcodec_open2() failed.\n";
        return 1;
    }

    AVStream* stream = avformat_new_stream(fmt_ctx, codec);
    if (!stream)
    {
        std::cerr << "avformat_new_stream() failed.\n";
        return 1;
    }

    stream->time_base = codec_ctx->time_base;
    stream->duration = 1;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = codec_ctx->codec_id;
    stream->codecpar->bit_rate = codec_ctx->bit_rate;
    stream->codecpar->width = codec_ctx->width;
    stream->codecpar->height = codec_ctx->height;

    ret = avio_open(&fmt_ctx->pb, "output.mp4", AVIO_FLAG_WRITE);
    if (ret < 0)
    {
        std::cerr << "avio_open() failed.\n";
        return 1;
    }

    ret = avformat_write_header(fmt_ctx, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_write_header() failed.\n";
        return 1;
    }

    std::cout << "Processing " << filtered_files.size() << " files ...\n";
    size_t frame_count = 0;
    AVFrame* frame = av_frame_alloc();
    AVPacket pkt = {0};

    for (auto const& file : filtered_files)
    {
        fs::path const& path = file.path();
        std::cout << "\r" << path.filename().string() << " ... \x1b[K" << std::flush;
        int w, h;
        std::vector<iteration_count_t> const& data = load_result(path.string(), w, h, max_iterations);
        if (w != width)
        {
            std::cerr << "frame " << frame_count << " differs in width\n";
        }
        if (h != height)
        {
            std::cerr << "frame " << frame_count << " differs in height\n";
        }
        std::cout << "\b\b\b\bloaded (" << width << "x" << height << ").";
        sf::Image const& img = colorize(data, width, height, height, max_iterations, get_rainbow_color);
        const sf::Uint8* const rgba_data = img.getPixelsPtr();
        frame->format = codec_ctx->pix_fmt;
        frame->pts = frame_count++;
        frame->width = codec_ctx->width;
        frame->height = codec_ctx->height;
        frame->quality = 1;
        frame->time_base = codec_ctx->time_base;
        frame->linesize[0] = width;     // Y plane
        frame->linesize[1] = width / 2; // U plane
        frame->linesize[2] = width / 2; // V plane
        int ret;
        ret = av_frame_make_writable(frame);
        ret = av_image_alloc(frame->data, frame->linesize, width, height, codec_ctx->pix_fmt, 1);
        if (ret < 0)
        {
            std::cerr << "Error in av_image_alloc()\n";
        }
        RGBAtoYUV420P(rgba_data, frame->data[0], frame->data[1], frame->data[2], codec_ctx->width, codec_ctx->height,
                      frame->linesize[0], frame->linesize[1]);
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0)
        {
            std::cerr << "Error in avcodec_send_frame()\n";
        }
        while (ret >= 0)
        {
            ret = avcodec_receive_packet(codec_ctx, &pkt);
            if (ret == 0)
            {
                ret = av_write_frame(fmt_ctx, &pkt);
            }
        }
    }

    av_packet_unref(&pkt);
    av_frame_free(&frame);
    av_write_trailer(fmt_ctx);
    avformat_free_context(fmt_ctx);
    avcodec_free_context(&codec_ctx);

    return 0;
}
