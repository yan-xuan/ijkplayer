//
// Created by yx on 2021/10/23.
//

#ifndef IJKPLAYER_IMAGEUTILS_H
#define IJKPLAYER_IMAGEUTILS_H

#include "ff_ffplay_def.h"
#include <libavutil/frame.h>


bool SaveImage(struct AVFrame *frame, const char *filePath);



#endif //IJKPLAYER_IMAGEUTILS_H
