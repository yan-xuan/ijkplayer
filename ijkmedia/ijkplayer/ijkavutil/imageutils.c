//
// Created by yx on 2021/10/23.
//

#include "imageutils.h"
#include "libavutil/pixfmt.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/avcodec.h"



bool ConvertAVFrame(const AVFrame * srcAVFrame,
                    enum AVPixelFormat dstPixelFormat,
                    int dstWidth,
                    int dstHeight,
                    AVFrame **dstAVFrame)
{
    if (srcAVFrame == NULL || dstAVFrame == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Input param is invalid!, %p, %p", srcAVFrame, dstAVFrame);
        return false;
    }

    if (dstPixelFormat == AV_PIX_FMT_NONE) {
        av_log(NULL, AV_LOG_ERROR, "Input param 'dstPixelFormat' is invalid!, value: %d", dstPixelFormat);
        return false;
    }

    if(dstWidth <= 0)
        dstWidth = srcAVFrame->width;
    if(dstHeight <= 0)
        dstHeight = srcAVFrame->height;

    if (srcAVFrame->format == dstPixelFormat && srcAVFrame->width == dstWidth && srcAVFrame->height == dstHeight) {
        // do nothing
        *dstAVFrame = av_frame_clone(srcAVFrame);
        return true;
    }

    struct SwsContext * scaleCtx = sws_getContext(srcAVFrame->width,
                                                  srcAVFrame->height,
                                                  (enum AVPixelFormat)srcAVFrame->format,
                                                  dstWidth,
                                                  dstHeight,
                                                  dstPixelFormat,
                                                  SWS_FAST_BILINEAR,
                                                  NULL,
                                                  NULL,
                                                  NULL);
    if (!scaleCtx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to get scale context!");
        return false;
    }

    AVFrame * scaleFrame = av_frame_alloc();

    // copy some property
    scaleFrame->format         = (int)dstPixelFormat;
    scaleFrame->width          = dstWidth;
    scaleFrame->height         = dstHeight;
    scaleFrame->channels       = srcAVFrame->channels;
    scaleFrame->channel_layout = srcAVFrame->channel_layout;
    scaleFrame->nb_samples     = srcAVFrame->nb_samples;

    // 根据上面的属性给 AVFrame 对象分配内存, 在 AVFrame 对象释放时自动回收
    av_frame_get_buffer(scaleFrame, 16);

    // copy matedata
    int ret = av_frame_copy_props(scaleFrame, srcAVFrame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to av_frame_copy_props!");
        av_frame_free(&scaleFrame);
        scaleFrame = NULL;

        sws_freeContext(scaleCtx);
        scaleCtx = NULL;
        return false;
    }

    ret = sws_scale(scaleCtx,
                    (const uint8_t* const*)srcAVFrame->data,
                    srcAVFrame->linesize,
                    0,
                    dstHeight,
                    scaleFrame->data,
                    scaleFrame->linesize);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "sws_scale failed. ret: %d", ret);
        av_frame_free(&scaleFrame);
        scaleFrame = NULL;

        sws_freeContext(scaleCtx);
        scaleCtx = NULL;
        return false;
    }

    // succeed
    sws_freeContext(scaleCtx);
    scaleCtx = NULL;

    // 调用者需要自行去释放该对象
    *dstAVFrame = scaleFrame;

    return true;
}



bool SaveImage(struct AVFrame *frame, const char *filePath)
{
    if (frame == NULL || filePath == NULL || strlen(filePath) < 4) {
        av_log(NULL, AV_LOG_ERROR, "input param is invalid! file path: %s, frame: %p\n", filePath, frame);
        return false;
    }

    if (frame->width <= 0 || frame->height <= 0) {
        av_log(NULL, AV_LOG_ERROR, "frame is invalid! w x h: %d x %d\n", frame->width, frame->height);
        return false;
    }

    char errString[128] = {};
    // Create output format context
    struct AVFormatContext *outputFormatCtx = NULL;
    int ret = avformat_alloc_output_context2(&outputFormatCtx, NULL, NULL, filePath);
    if (ret < 0) {
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "avformat_alloc_output_context2() for '%s' failed! error string='%s'", filePath, errString);
        outputFormatCtx = NULL;
        return false;
    }

    AVStream *stream = avformat_new_stream(outputFormatCtx, NULL);
    if (stream == NULL) {
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "avformat_new_stream() for '%s' failed! error string='%s'", filePath, errString);
        avformat_free_context(outputFormatCtx);
        outputFormatCtx = NULL;
        return false;
    }

// #ifdef __DEBUG__
//     av_dump_format(outputFormatCtx, 0, filePath, 1);
// #endif

    const enum AVCodecID codecID = outputFormatCtx->oformat->video_codec;

    AVCodec* pCodec = NULL;
    pCodec = avcodec_find_encoder(codecID);
    if (!pCodec){
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "avcodec_find_encoder() for '%s' failed! error string='%s'", filePath, errString);

        if (outputFormatCtx) {
            if (outputFormatCtx->pb)
                avio_close(outputFormatCtx->pb);
            avformat_free_context(outputFormatCtx);
            outputFormatCtx = NULL;
        }

        stream = NULL;
        return false;
    }

    enum AVPixelFormat pixelFormat = (enum AVPixelFormat)frame->format;

    // 获取其支持的Pixel Format
    bool isSupportedPixFmt = false;
    const enum AVPixelFormat *pix_fmts = pCodec->pix_fmts;
    while (pix_fmts != NULL) {
        enum AVPixelFormat avPixFmt = *pix_fmts;
        if (avPixFmt >= AV_PIX_FMT_NB || avPixFmt <= AV_PIX_FMT_NONE)
            break;

        if (avPixFmt == pixelFormat) {
            isSupportedPixFmt = true;
            break;
        }

        pix_fmts += sizeof(enum AVPixelFormat);
    }



    int imageWidth = frame->width;
    int imageHeight = frame->height;

    bool needReclaimMem = false;
    AVFrame *avFrame = frame;
    enum AVPixelFormat encodePixFmt = pixelFormat;
    if (!isSupportedPixFmt) {    // convert pixel format
        avFrame = NULL;
        encodePixFmt = AV_PIX_FMT_YUV420P;
        bool succ = ConvertAVFrame(frame, encodePixFmt, imageWidth, imageHeight, &avFrame);
        if (!succ) {
            av_log(NULL, AV_LOG_ERROR, "Convert color is failed!");
            if (outputFormatCtx) {
                if (outputFormatCtx->pb)
                    avio_close(outputFormatCtx->pb);
                avformat_free_context(outputFormatCtx);
                outputFormatCtx = NULL;
            }

            return false;
        }

        needReclaimMem = true;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(pCodec);
    codecCtx->codec_id = codecID;
    codecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    codecCtx->pix_fmt = encodePixFmt;

    codecCtx->width = avFrame->width;
    codecCtx->height = avFrame->height;

    codecCtx->time_base.num = 1;
    codecCtx->time_base.den = 25;

    ret = avcodec_open2(codecCtx, pCodec, NULL);
    if (ret < 0) {
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "avcodec_open2() for '%s' failed! error string='%s'", filePath, errString);

        if (stream && codecCtx && avcodec_is_open(codecCtx)) {
            avcodec_free_context(&codecCtx);
            stream = NULL;
        }

        if (outputFormatCtx) {
            if (outputFormatCtx->pb)
                avio_close(outputFormatCtx->pb);
            avformat_free_context(outputFormatCtx);
            outputFormatCtx = NULL;
        }

        return false;
    }

    avcodec_parameters_from_context(stream->codecpar, codecCtx);

    // 控制压缩质量
    if (codecCtx->codec_id == AV_CODEC_ID_MJPEG ) {
        codecCtx->qcompress = 1.0f; // 0~1.0, default is 0.5
        codecCtx->qmin = 2;
        codecCtx->qmax = 31;
        codecCtx->max_qdiff = 3;
    }

    ret = avformat_write_header(outputFormatCtx, NULL);
    if (ret) {
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "Write file header is failed! errno: %d, error: '%s'", ret, errString);

        if (stream && codecCtx && avcodec_is_open(codecCtx)) {
            avcodec_free_context(&codecCtx);
            stream = NULL;
        }

        if (outputFormatCtx) {
            if (outputFormatCtx->pb)
                avio_close(outputFormatCtx->pb);
            avformat_free_context(outputFormatCtx);
            outputFormatCtx = NULL;
        }

        if (needReclaimMem && avFrame != NULL) {
            av_frame_free(&avFrame);
            avFrame = NULL;
        }

        return false;
    }

    //
    // Encode this frame
    //
    AVPacket videoPacket;
    videoPacket.data = NULL;
    videoPacket.size = 0;
    av_init_packet(&videoPacket);

    ret = avcodec_send_frame(codecCtx, avFrame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "avcodec_encode_video2() failed for '%s'! error string='%s'", filePath, errString);
        av_frame_free(&avFrame);

        if (stream && codecCtx && avcodec_is_open(codecCtx)) {
            avcodec_free_context(&codecCtx);
            stream = NULL;
        }

        if (outputFormatCtx) {
            if (outputFormatCtx->pb)
                avio_close(outputFormatCtx->pb);
            avformat_free_context(outputFormatCtx);
            outputFormatCtx = NULL;
        }

         if (needReclaimMem) {
             av_frame_free(&avFrame);
             avFrame = NULL;
         }

        return false;
    }

    while (true) {
        ret = avcodec_receive_packet(codecCtx, &videoPacket);
        if (!ret) {
            break;
        }

        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            av_strerror(ret, errString, 128);
            av_log(NULL, AV_LOG_ERROR, "avcodec_encode_video2() failed for '%s'! error string='%s'", filePath, errString);
            av_frame_free(&avFrame);

            if (stream && codecCtx && avcodec_is_open(codecCtx)) {
                avcodec_free_context(&codecCtx);
                stream = NULL;
            }

            if (outputFormatCtx) {
                if (outputFormatCtx->pb)
                    avio_close(outputFormatCtx->pb);
                avformat_free_context(outputFormatCtx);
                outputFormatCtx = NULL;
            }

            if (needReclaimMem && avFrame != NULL) {
                av_frame_free(&avFrame);
                avFrame = NULL;
            }

            return false;
        }
    }

    // 至此, frame 已使用完成, 若为自己分别的需要将其释放
    if (needReclaimMem && avFrame != NULL) {
        av_frame_free(&avFrame);
        avFrame = NULL;
    }

    if (videoPacket.size) {
        videoPacket.stream_index = stream->index;
        videoPacket.pts = av_rescale_q(videoPacket.pts, codecCtx->time_base, stream->time_base);
        videoPacket.dts = av_rescale_q(videoPacket.dts, codecCtx->time_base, stream->time_base);
        videoPacket.duration = 0;//av_rescale_q(1, videoEncoderCtx->time_base, m_videoStream->time_base);

        // Write the compressed video frame to the media file
        ret = av_write_frame(outputFormatCtx, &videoPacket);
        if (ret < 0) {
            av_strerror(ret, errString, 128);
            av_log(NULL, AV_LOG_ERROR, "av_interleaved_write_frame() failed for '%s'! error string='%s'", filePath, errString);
            av_packet_unref(&videoPacket);

            if (stream && codecCtx && avcodec_is_open(codecCtx)) {
                avcodec_free_context(&codecCtx);
                stream = NULL;
            }

            if (outputFormatCtx) {
                if (outputFormatCtx->pb)
                    avio_close(outputFormatCtx->pb);
                avformat_free_context(outputFormatCtx);
                outputFormatCtx = NULL;
            }

            if (needReclaimMem && avFrame != NULL) {
                av_frame_free(&avFrame);
                avFrame = NULL;
            }

            return false;
        }
    }

    av_packet_unref(&videoPacket);

    // flush
    ret = av_write_trailer(outputFormatCtx);
    if (ret) {
        char errString[128];
        av_strerror(ret, errString, 128);
        av_log(NULL, AV_LOG_ERROR, "av_write_trailer() failed for '%s'! error string='%s'", filePath, errString);

        if (stream && codecCtx && avcodec_is_open(codecCtx)) {
            avcodec_free_context(&codecCtx);
            stream = NULL;
        }

        if (outputFormatCtx) {
            if (outputFormatCtx->pb)
                avio_close(outputFormatCtx->pb);
            avformat_free_context(outputFormatCtx);
            outputFormatCtx = NULL;
        }

        return false;
    }

    // 资源回收
    if (stream && codecCtx && avcodec_is_open(codecCtx)) {
        avcodec_free_context(&codecCtx);
        stream = NULL;
    }

    if (outputFormatCtx) {
        if (outputFormatCtx->pb)
            avio_close(outputFormatCtx->pb);
        avformat_free_context(outputFormatCtx);
        outputFormatCtx = NULL;
    }

    return true;
}






// bool SaveImage(AVFrame *frame, const char *filePath)
// {
//     AVCodecID id = AV_CODEC_ID_NONE;
//     // AVPixelFormat pixel;
//     // pixel = AV_PIX_FMT_YUV420P;
//     return false;
// }


