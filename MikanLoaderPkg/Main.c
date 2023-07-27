#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>

/* メモリマップ構造体を定義 */
struct MemoryMap
{
  UINTN buffer_size;      // メモリマップ書き込み用のメモリの大きさ（UINTNはunsigned intの4バイト)
  VOID* buffer;           // メモリマップ書き込み用のメモリ領域の先頭ポインタ
  UINTN map_size;         // 実際のメモリマップの大きさ
  UINTN map_key;          // メモリマップを色部宇するための値格納用の変数
  UINTN descriptor_size;  // メモリディスクリプタのサイズ
  UINT32 descriptor_version;  //メモリディスクリプタの構造体のバージョン番号
};


/* メモリマップ取得用の関数 */
EFI_STATUS GetMemoryMap(struct MemoryMap* map)
{
  if (map->buffer == NULL)
  {
    return EFI_BUFFER_TOO_SMALL;
  }

  map->map_size = map->buffer_size;
  return gBS->GetMemoryMap(
    &map->map_size,
    (EFI_MEMORY_DESCRIPTOR*)map->buffer,
    &map->map_key,
    &map->descriptor_size,
    &map->descriptor_version
  );
}


/* メモリ領域のタイプを文字列(ワイド文字)に変換数する関数 */
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType: return L"EfiReservedMemoryType";
    case EfiLoaderCode: return L"EfiLoaderCode";
    case EfiLoaderData: return L"EfiLoaderData";
    case EfiBootServicesCode: return L"EfiBootServicesCode";
    case EfiBootServicesData: return L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return L"EfiConventionalMemory";
    case EfiUnusableMemory: return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return L"EfiPalCode";
    case EfiPersistentMemory: return L"EfiPersistentMemory";
    case EfiMaxMemoryType: return L"EfiMaxMemoryType";
    default: return L"InvalidMemoryType";
  }
}


/* メモリ領域に書き込んだメモリマップをファイルに保存する用の関数 */
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file)
{
  CHAR8 buf[256];   // 1行分のメモリマップ情報を書き込みする用のバッファ
  UINTN len;        // 1行分の書き込みデータのサイズ格納用の変数

  /* ファイルの先頭に付けるヘッダー情報を書き込む */
  CHAR8* header =
    "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n"; // ヘッダーの文字列作成
  len = AsciiStrLen(header);  // ヘッダーの文字列のサイズを取得
  file->Write(file, &len, header);  // ファイルにヘッダー情報を書き込む

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
    map->buffer, map->map_size
  );

  EFI_PHYSICAL_ADDRESS iter;  // メモリマップの各メモリディスクリプタの先頭アドレスを指すポインタ
  int i;  // メモリマップの行番号のカウンタ(Index)
  for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
       iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
       iter += map->descriptor_size, i++)
  {
    /* メモリディスクリプタ型のポインタにキャストして中身を読み出す　*/
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    /* メモリディスクリプタ中身を指定したフォーマットにしたがってbufに書き出す */
    len = AsciiSPrint(
        buf, sizeof(buf),
        "%u, %x, %-ls, %08lx, %lx, %lx\n",
        i, desc->Type, GetMemoryTypeUnicode(desc->Type),
        desc->PhysicalStart, desc->NumberOfPages,
        desc->Attribute & 0xffffflu
    );
    /* bufの内容をファイルに書き込む */
    file->Write(file, &len, buf);
  }

  return EFI_SUCCESS;
}


/* ルート(ボリューム)をオープンする関数                             */
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root)
{
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

  /* OpenProtocolでEFI_LOADED_IMAGE_PROTOCOLをOpen */
  gBS->OpenProtocol(
    image_handle,
    &gEfiLoadedImageProtocolGuid,
    (VOID**)&loaded_image,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );

  /* OpenProtocolでEFI_LOADED_IMAGE_PROTOCOLのDeviceHandleをOpen */
  gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiSimpleFileSystemProtocolGuid,
    (VOID**)&fs,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );

  /* OpenVolumeでルート(ボリューム)をOpen */
  fs->OpenVolume(fs, root);

  return EFI_SUCCESS;
}


EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table) {
  Print(L"Hello, Mikan World!\n");

  CHAR8 memmap_buf[4096 * 4];   //メモリマップ書き込み用のメモリ領域を確保する(16KiB)
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};  // メモリマップ構造体を作成
  GetMemoryMap(&memmap);  // メモリマップをメモリ領域に書き込む
  Print(L"GetMemoryMap Done\n");

  /* ルート(ボリューム)をオープンする */
  EFI_FILE_PROTOCOL* root_dir;
  OpenRootDir(image_handle, &root_dir);
  Print(L"OpenRootDir Done\n");

  /* メモリマップを書き込む用のファイル"memmap"を作成し、memmapfileにオープン */
  EFI_FILE_PROTOCOL* memmap_file;
  root_dir->Open(
    root_dir, &memmap_file, L"\\memmap",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
  );
  Print(L"root_dir->Open Done\n");
  
  SaveMemoryMap(&memmap, memmap_file);  // メモリ領域に書き込まれたメモリマップをファイルに書き込む
  memmap_file->Close(memmap_file);  // memmap_fileを閉じる。
  Print(L"SaveMemoryMap\n");

  Print(L"All done\n");

  while (1);
  return EFI_SUCCESS;
}
