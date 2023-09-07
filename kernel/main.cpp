#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"

/* 1ピクセルのRGBを制御する用の構造体を定義する */
struct PixelColor {
    uint8_t r, g, b;
};

/* 1ピクセルを指定した色で塗る関数（戻り値 0：成功、0以外：失敗） */
int WritePixel(const FrameBufferConfig& config,
                int x, int y, const PixelColor& color) {
    const int pixel_position = config.pixels_per_scan_line * y + x; // 指定されたピクセルが先頭から何番目の位置にあるかを計算
    uint8_t* p = &config.frame_buffer[4 * pixel_position];      // 指定されたピクセルのフレームのアドレスをPに格納
    /* ピクセルのデータ形式がRGBの場合 */
    if (config.pixel_format == kPixelRGBResv8VitPerColor) {
        p[0] = color.r;     // ピクセルのフレームに赤の設定値を書き込み
        p[1] = color.g;     // ピクセルのフレームに緑の設定値を書き込み
        p[2] = color.b;     // ピクセルのフレームに青の設定値を書き込み
    }
    /* ピクセルのデータ形式がBGRの場合 */
    if (config.pixel_format == kPixelBGRResv8VitPerColor) {
        p[0] = color.b;     // ピクセルのフレームに青の設定値を書き込み
        p[1] = color.g;     // ピクセルのフレームに緑の設定値を書き込み
        p[2] = color.r;     // ピクセルのフレームに赤の設定値を書き込み
    }
    /* それ以外の場合 */
    else {
        return -1;  // 失敗
    }
    return 0;   // 成功
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config)
{
    /* 一旦、画面を白で塗りつぶす */
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {  // 横（0～最後まで）
        for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) { // 縦（0～最後まで）
            WritePixel(frame_buffer_config, x, y, {255, 255, 255}); //　該当ピクセルを白で塗りつぶす
        }
    }

    /* 指定した位置を緑に塗る*/
    for (int x = 0; x < 200; ++x) {     // 横（0～199まで）
        for (int y = 0; y < 100; ++y) {     // 横（0～99まで）
            WritePixel(frame_buffer_config, 100 + x, 100 + y, {0, 255, 0}); // 開始位置から横に100+x、縦に100+yの位置を緑に塗る
        }
    }
    while(1) __asm__("hlt");
}