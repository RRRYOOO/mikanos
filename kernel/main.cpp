#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"

/* 1ピクセルのRGBを制御する用の構造体を定義する */
struct PixelColor {
    uint8_t r, g, b;
};

/* PixelWriterの親クラス */
class PixelWriter {
 public:
  PixelWriter(const FrameBufferConfig& config) : config_{config} {      // コンストラクタ
  }
  virtual ~PixelWriter() = default;     // デストラクタ
  virtual void Write(int x, int y, const PixelColor& c) = 0;       // ピクセル描画関数

 protected:
  uint8_t* PixelAt(int x, int y) {
    return config_.frame_buffer + 4 * (config_.pixels_per_scan_line * y + x);        // 指定されたピクセルのフレームのアドレスを返す
  }

 private:
  const FrameBufferConfig& config_;     // フレームバッファ情報
};

/* PixelWriterの子クラス（ピクセルのデータ形式がRGBの場合） */
class RGBResv8BitPerColorPixelWriter : public PixelWriter {
 public:
  using PixelWriter::PixelWriter;    // 親クラスのコンストラクタをそのまま使用

  virtual void Write(int x, int y, const PixelColor& c) override {      // 親クラスのピクセル描画関数をオーバーライド
    auto p = PixelAt(x, y);     // 指定されたピクセルのフレームアドレスを取得
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
  }
};

/* PixelWriterの子クラス（ピクセルのデータ形式がBGRの場合） */
class BGRResv8BitPerColorPixelWriter : public PixelWriter {
 public:
  using PixelWriter::PixelWriter;    // 親クラスのコンストラクタをそのまま使用

  virtual void Write(int x, int y, const PixelColor& c) override {      // 親クラスのピクセル描画関数をオーバーライド
    auto p = PixelAt(x, y);     // 指定されたピクセルのフレームアドレスを取得
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
  }
};

/* 配置new用の関数 */
void* operator new(size_t size, void* buf) {
    return buf;     // 引数で受け取った領域のアドレスをそのままかえるだけ
}
/* delete用の関数（使用していないけど、デストラクタの中で要求されるため） */
void operator delete(void* obj) noexcept {
}

/* クラスのインスタンス用に領域を確保する */
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
/* クラスのインスタンスのポインタの格納用（この変数を用いて操作を行う） */
PixelWriter* pixel_writer;

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    switch (frame_buffer_config.pixel_format) {     // ピクセルのデータ形式によってインスタンス化するクラスを分ける
      case kPixelRGBResv8BitPerColor:       // ピクセルのデータ形式がRGBの場合
        pixel_writer = new(pixel_writer_buf)        // クラスのインスタンス用に確保した領域を割り当てる
          RGBResv8BitPerColorPixelWriter{frame_buffer_config};      // RGB用の子クラスをインスタンス化する
        break;
      case kPixelBGRResv8BitPerColor:       // ピクセルのデータ形式がBGRの場合
        pixel_writer = new(pixel_writer_buf)        // クラスのインスタンス用に確保した領域を割り当てる
          BGRResv8BitPerColorPixelWriter{frame_buffer_config};      // BGR用の子クラスをインスタンス化する
        break;
    }
    /* 一旦、画面を白で塗りつぶす */
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {  // 横（0～最後まで）
        for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) { // 縦（0～最後まで）
            pixel_writer->Write(x, y, {255, 255, 255}); //　該当ピクセルを白で塗りつぶす
        }
    }
    /* 指定した位置を緑に塗る*/
    for (int x = 0; x < 200; ++x) {     // 横（0～199まで）
        for (int y = 0; y < 100; ++y) {     // 横（0～99まで）
            pixel_writer->Write(x, y, {0, 255, 0}); // 開始位置から横にx、縦にyの位置を緑に塗る
        }
    }
    while(1) __asm__("hlt");
}