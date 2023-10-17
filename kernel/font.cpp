#include "font.hpp"

extern const uint8_t _binary_hankaku_bin_start;   // フォントデータの開始アドレス
extern const uint8_t _binary_hankaku_bin_end;     // フォントデータの終了アドレス+1
extern const uint8_t _binary_hankaku_bin_size;    // フォントデータのサイズ

/* 指定した文字のフォントデータの格納先アドレスを返す関数 */
const uint8_t* GetFont(char c) {
  auto index = 16 * static_cast<unsigned int>(c);   // 引数cの文字をASCIIコードに変換後に、×16してフォントデータの先頭アドレスを算出
  if (index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) {  // indexの値がフォントデータの全サイズより大きい場合（エラー）
    return nullptr;
  }
  return &_binary_hankaku_bin_start + index;    // フォントデータの開始アドレスからindexだけ進んだアドレスを返す
}

/* フォント描画用の関数 */
void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color) {
  const uint8_t* font = GetFont(c);   // 指定した文字のフォントデータの格納先アドレスを取得
  if (font == nullptr) {    // アドレス読み出し失敗
    return;
  }
  for (int dy = 0; dy < 16; ++dy) {
    for (int dx = 0; dx < 8; ++dx) {
      if (font[dy] << dx & 0x80u) {
        writer.Write(x + dx, y + dy, color);    // 指定した位置のピクセルを指定したカラーで描画
      }
    }
  }
}

/* 文字列描画用の変数 */
void WriteString(PixelWriter& writer, int x, int y, const char* s, const PixelColor& color) {
  /* 文字列の分だけfor文で繰り返してWriteAsciiをコールする */
  for (int i = 0; s[i] != '\0'; ++i) {
    WriteAscii(writer, x + 8*i, y, s[i], color);
  }
}