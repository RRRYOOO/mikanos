#include <cstdint>

extern "C" void KernelMain(uint64_t frame_buffer_base,
                           uint64_t frame_buffer_size)
{
    /* フレームバッファの先頭アドレスを8bit(1Byte)型のポインタ型に変換 */
    uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(frame_buffer_base);
    for(uint64_t i = 0; i < frame_buffer_size; ++i) {
        /* フレームバッファに0～255の値を格納 */
        frame_buffer[i] = i % 256;
    }
    while(1) __asm__("hlt");
}