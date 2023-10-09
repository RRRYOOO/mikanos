#include "graphics.hpp"

/* RGBResv8BitPerColorPixelWriter()のメンバ関数Write()をオーバーライド */
void RGBResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& c) {
    auto p = PixelAt(x, y);     // 指定されたピクセルのフレームアドレスを取得
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
}

/* BGRResv8BitPerColorPixelWriter()のメンバ関数Write()をオーバーライド */
void BGRResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& c) {
    auto p = PixelAt(x, y);     // 指定されたピクセルのフレームアドレスを取得
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
}