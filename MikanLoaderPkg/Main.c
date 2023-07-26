#include  <Uefi.h>
#include  <Library/UefiLib.h>

// メモリマップ構造体を定義
struct MemoryMap
{
  UINTN buffer_size;    // メモリマップ書き込み用のメモリの大きさ（UINTNはunsigned intの4バイト)
  VOID* buffer          // メモリマップ書き込み用のメモリ領域の先頭ポインタ
  UINTN map_size;       // 実際のメモリマップの大きさ
  UINTN map_key;        // メモリマップを色部宇するための値格納用の変数
  UINTN descriptor_size // メモリディスクリプタのサイズ
  UINT32 descriptor_version;  //メモリディスクリプタの構造体のバージョン番号
};

// メモリマップ取得用の関数
EFI_STATUS GetMemoryMap(struct MemoryMap *map)
{
  if (map->buffer == NULL)
  {
    return EFI_BUFFER_TOO_SMALL;
  }

  map->mapsize = map->buffer_size;
  return gBS->GetMemoryMap(
    &map->mapsize,
    (EFI_MEMORY_DESCRIPTOR*)map->buffer,
    &map->map_key,
    &map->descriptor_size,
    &map->descriptor_version
  );
}

EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table) {
  Print(L"Hello, Mikan World!\n");

  CHAR8 memmap_buf[4096 * 4];   //メモリマップ書き込み用のメモリ領域を確保する(16KiB)
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0}  // メモリマップ構造体を作成
  GetMemoryMap(&memmap);

  EFI_FILE_PROTOCOL* root_dir;
  root_dir->Open(
    root_dir, &memmap_file, L"\\memmap",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  );

  while (1);
  return EFI_SUCCESS;
}
