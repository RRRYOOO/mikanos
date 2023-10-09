#pragma once

#include <cstdint>
#include "graphics.hpp"

/* 'A'のフォント描画用の関数(プロトタイプ宣言) */
void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color);