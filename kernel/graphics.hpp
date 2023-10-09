#pragma once

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
  virtual void Write(int x, int y, const PixelColor& c) override;       // 親クラスのピクセル描画関数をオーバーライド(定義のみ)
};

/* PixelWriterの子クラス（ピクセルのデータ形式がBGRの場合） */
class BGRResv8BitPerColorPixelWriter : public PixelWriter {
 public:
  using PixelWriter::PixelWriter;    // 親クラスのコンストラクタをそのまま使用
  virtual void Write(int x, int y, const PixelColor& c) override;       // 親クラスのピクセル描画関数をオーバーライド(定義のみ)
};