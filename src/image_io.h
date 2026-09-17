#pragma once
#include "encoder.h"
#include <string>

namespace codec {

// يقرأ أي صيغة (PNG/JPG/BMP/TGA/PGM/PPM) تلقائياً
Image readImageAuto(const std::string& path);

// يكتب حسب الامتداد (.png/.jpg/.bmp/.tga/.pgm/.ppm)
void  writeImageAuto(const std::string& path, const Image& img);

} // namespace codec