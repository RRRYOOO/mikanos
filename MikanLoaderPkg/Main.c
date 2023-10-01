#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/BaseMemoryLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>
#include  "frame_buffer_config.hpp"
#include  "elf.hpp"

/* メモリマップ構造体を定義 */
struct MemoryMap {
  UINTN buffer_size;      // メモリマップ書き込み用のメモリの大きさ（UINTNはunsigned intの4バイト)
  VOID* buffer;           // メモリマップ書き込み用のメモリ領域の先頭ポインタ
  UINTN map_size;         // 実際のメモリマップの大きさ
  UINTN map_key;          // メモリマップを色部宇するための値格納用の変数
  UINTN descriptor_size;  // メモリディスクリプタのサイズ
  UINT32 descriptor_version;  //メモリディスクリプタの構造体のバージョン番号
};

/* メモリマップ取得用の関数 */
EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
  if (map->buffer == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  map->map_size = map->buffer_size;
  return gBS->GetMemoryMap(
      &map->map_size,
      (EFI_MEMORY_DESCRIPTOR*)map->buffer,
      &map->map_key,
      &map->descriptor_size,
      &map->descriptor_version);
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
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
  EFI_STATUS status; // 実行結果を返す用の変数
  CHAR8 buf[256];   // 1行分のメモリマップ情報を書き込みする用のバッファ
  UINTN len;        // 1行分の書き込みデータのサイズ格納用の変数

  /* ファイルの先頭に付けるヘッダー情報を書き込む */
  CHAR8* header =
    "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n"; // ヘッダーの文字列作成
  len = AsciiStrLen(header);  // ヘッダーの文字列のサイズを取得
  status = file->Write(file, &len, header);  // ファイルにヘッダー情報を書き込む
  /* ヘッダー情報のファイルへの書き込みに失敗した場合 */
  if (EFI_ERROR(status)) {
    return status;
  }

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
      map->buffer, map->map_size);

  EFI_PHYSICAL_ADDRESS iter;  // メモリマップの各メモリディスクリプタの先頭アドレスを指すポインタ
  int i;  // メモリマップの行番号のカウンタ(Index)
  for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
       iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
       iter += map->descriptor_size, i++) {
    /* メモリディスクリプタ型のポインタにキャストして中身を読み出す　*/
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    /* メモリディスクリプタ中身を指定したフォーマットにしたがってbufに書き出す */
    len = AsciiSPrint(
        buf, sizeof(buf),
        "%u, %x, %-ls, %08lx, %lx, %lx\n",
        i, desc->Type, GetMemoryTypeUnicode(desc->Type),
        desc->PhysicalStart, desc->NumberOfPages,
        desc->Attribute & 0xffffflu);
    /* bufの内容をファイルに書き込む */
    status = file->Write(file, &len, buf);
    /* bufの内容のファイルへの書き込みに失敗した場合 */
    if (EFI_ERROR(status)) {
      return status;
    }
  }

  return EFI_SUCCESS;
}

/* ルート(ボリューム)をオープンする関数 */
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
  EFI_STATUS status; // 実行結果を返す用の変数
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

  /* OpenProtocolでEFI_LOADED_IMAGE_PROTOCOLをOpen */
  status = gBS->OpenProtocol(
      image_handle,
      &gEfiLoadedImageProtocolGuid,
      (VOID**)&loaded_image,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)) {
    return status;
  }

  /* OpenProtocolでEFI_LOADED_IMAGE_PROTOCOLのDeviceHandleをOpen */
  status = gBS->OpenProtocol(
      loaded_image->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID**)&fs,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)) {
    return status;
  }

  /* OpenVolumeでルート(ボリューム)をOpen */
  return fs->OpenVolume(fs, root);
}

/* GOPをオープンする関数 */
EFI_STATUS OpenGOP(EFI_HANDLE image_handle,
                   EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
  EFI_STATUS status; // 実行結果を返す用の変数
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = NULL;

  status = gBS->LocateHandleBuffer(
      ByProtocol,
      &gEfiGraphicsOutputProtocolGuid,
      NULL,
      &num_gop_handles,
      &gop_handles);
  if (EFI_ERROR(status)) {
    return status;
  }

  status = gBS->OpenProtocol(
      gop_handles[0],
      &gEfiGraphicsOutputProtocolGuid,
      (VOID**)gop,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)) {
    return status;
  }

  FreePool(gop_handles);

  return EFI_SUCCESS;
}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
  switch (fmt) {
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return L"PixelBitMask";
    case PixelBltOnly:
      return L"PixelBltOnly";
    case PixelFormatMax:
      return L"PixelFormatMax";
    default:
      return L"InvalidPixelFormat";
  }
}


/* エラー発生時の停止用の関数 */
void Halt(void) {
  while(1)  __asm("hlt");
}

/* カーネルファイルのLOAD領域格納用メモリのサイズを計算する関数CalcLoadAddressRange() */
void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last) {
  /* カーネルファイルのプログラムヘッダ領域を64ビットELFのプログラムヘッダ構造体型にキャストする */
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  *first = MAX_UINT64;  // カーネルファイルのLOAD領域格納用メモリの開始アドレスを設定する
  *last = 0;  // カーネルファイルのLOAD領域格納用メモリの終了アドレスを設定する
  /* カーネルファイル内のすべてのLOADセグメントをfor文で順番に確認していき、開始アドレス(first)と終了アドレス(last)を更新していく */
  for(Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
    /* プログラムヘッダの配列のタイプがLOADセグメントでない場合はスキップ*/
    if (phdr[i].p_type != PT_LOAD) continue;
    *first = MIN(*first, phdr[i].p_vaddr);
    *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
  }
}

/* 一時領域に展開したカーネルファイルのLOADセグメントをメモリに格納する関数CopyLoadSegments() */
void CopyLoadSegments(Elf64_Ehdr* ehdr) {
  /* カーネルファイルのプログラムヘッダ領域を64ビットELFのプログラムヘッダ構造体型にキャストする */
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  /* カーネルファイル内のすべてのLOADセグメントをfor文で順番に確認していき、メモリにロードしていく */
  for(Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
    /* プログラムヘッダの配列のタイプがLOADセグメントでない場合はスキップ*/
    if (phdr[i].p_type != PT_LOAD) continue;

    /* 一時領域側のアドレスを計算して、segm_in_fileに設定 */
    UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
    /* LOADセグメントを一時領域からメモリにコピー */
    CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segm_in_file, phdr[i].p_filesz);

    /* ファイル上でのサイズとメモリ上でのサイズの差を計算して、 remain_bytesにセット */
    UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
    /* セグメントのメモリ上のサイズがファイル上のサイズより大きい場合（remain_bytes > 0）、残りを0で埋める */
    SetMem((VOID*)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
  }
}

EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table) {
  EFI_STATUS status;
  Print(L"Hello, Mikan World!\n");

  /* メモリマップを取得 */
  CHAR8 memmap_buf[4096 * 4];   //メモリマップ書き込み用のメモリ領域を確保する(16KiB)
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};  // メモリマップ構造体を作成
  status = GetMemoryMap(&memmap);  // メモリマップをメモリ領域に書き込む
  /* メモリマップのメモリ領域への書き込みに失敗した場合 */
  if (EFI_ERROR(status)) {
    Print(L"failed to get memory map: %r\n", status);
    Halt();
  }

  /* ルートディレクトリ(ボリューム)をオープンする */
  EFI_FILE_PROTOCOL* root_dir;
  status = OpenRootDir(image_handle, &root_dir);
  /* ルートディレクトリのオープンに失敗した場合 */
  if (EFI_ERROR(status)) {
    Print(L"failed to open root directory: %r\n", status);
    Halt();
  }

  /* メモリマップを書き込む用のファイル"memmap"を作成し、memmap_fileにオープン */
  EFI_FILE_PROTOCOL* memmap_file;
  status = root_dir->Open(
      root_dir, &memmap_file, L"\\memmap",
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  /* "memmap"のオープンに失敗した場合 */
  if (EFI_ERROR(status)) {
    Print(L"failed to open file '\\memmap': %r\n", status);
    Print(L"Ignored.\n");
  } else {
    status = SaveMemoryMap(&memmap, memmap_file);  // メモリ領域に書き込まれたメモリマップをファイルに書き込む
    /* メモリマップのファイルへの書き込みに失敗した場合 */
    if (EFI_ERROR(status)) {
      Print(L"failed to save memory map: %r\n", status);
      Halt();
    }
    status = memmap_file->Close(memmap_file);  // memmap_fileを閉じる
    /* memmap_fileを閉じるのに失敗した場合 */
    if (EFI_ERROR(status)) {
      Print(L"failed to close memory map: %r\n", status);
      Halt();
    }
  }

  /* GOPを取得して画面描画する */
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  /* GOPを取得する */
  status = OpenGOP(image_handle, &gop);
  /* GOPの取得に失敗した場合 */
  if (EFI_ERROR(status)) {
    Print(L"failed to open GOP: %r\n", status);
    Halt();
  }

  Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
      gop->Mode->Info->HorizontalResolution,
      gop->Mode->Info->VerticalResolution,
      GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
      gop->Mode->Info->PixelsPerScanLine);
  Print(L"Frame Buffer: 0x%0lx - 0x%0lx, Size: %lu bytes\n",
      gop->Mode->FrameBufferBase,
      gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
      gop->Mode->FrameBufferSize);

  UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase; // フレームバッファの先頭アドレスをフレームバッファ型にキャスト
  for(UINTN i = 0; i < gop->Mode->FrameBufferSize; ++i) {
    frame_buffer[i] = 255;  // フレームバッファを白で埋める（画面が白一色になる）
  }

  /* カーネルファイルをオープン */
  EFI_FILE_PROTOCOL* kernel_file;
  status = root_dir->Open(
      root_dir, &kernel_file, L"\\kernel.elf",
      EFI_FILE_MODE_READ, 0);
  /* カーネルファイルのオープンに失敗した場合 */
  if (EFI_ERROR(status)) {
    Print(L"failed to open file '\\kernel.elf': %r\n", status);
    Halt();
  }

  /* カーネルファイルのファイル情報を読み出す */
  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
  UINT8 file_info_buffer[file_info_size];
  status = kernel_file->GetInfo(
      kernel_file, &gEfiFileInfoGuid,
      &file_info_size, file_info_buffer);
  /* カーネルファイルのファイル情報の読み出しに失敗した場合 */
  if(EFI_ERROR(status)) {
    Print(L"failed to get file information: %r\n", status);
    Halt();
  }

  /* カーネルファイル情報からカーネルファイルのサイズを取得 */
  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN kernel_file_size = file_info->FileSize;

  /* カーネルファイルを一時領域に読み込む */
  VOID* kernel_buffer;
  /* カーネルファイルを一時的に読み込むための領域(=kernel_buffer)を確保する */
  status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buffer);
  /* カーネルファイル格納用のメモリ確保に失敗した場合 */
  if(EFI_ERROR(status)) {
    Print(L"failed to allocate pool: %r\n", status);
    Halt();
  }
  /* カーネルファイルを一時領域(=kernel_buffer)に読み込む */
  status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buffer);
    /* カーネルファイルのメモリへの読み込みに失敗した場合 */
  if(EFI_ERROR(status)) {
    Print(L"error: %r\n", status);
    Halt();
  }

  /* 一時領域に読み込んだカーネルファイルのプログラムヘッダを読み、最終目的地の番地の範囲を取得する */
  /* 一時領域に読み込んだカーネルファイルを64ビットELFのファイルヘッダの構造体型にキャストする */
  Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
  UINT64 kernel_first_addr, kernel_last_addr;
  /* CalcLoadAddressRange()が最終目的地の番地の範囲を計算する */
  /* 範囲の開始アドレスをkernel_first_addrに、終了アドレスをkernel_last_addrに設定する */
  CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

  /* カーネルファイルのLOADセグメントを格納するのに必要なメモリメモリのサイズを計算する（ページ単位） */
  UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
  /* カーネルファイルのLOADセグメントを格納するためのメモリ領域を確保する */
  status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                              num_pages, &kernel_first_addr);
  /* カーネルファイルのLOADセグメントを格納するためのメモリ領域確保に失敗した場合 */
  if(EFI_ERROR(status)) {
    Print(L"failed to allocate pages: %r\n", status);
    Halt();
  }

  /* CopyLoadSegments()でLoadセグメントを一括でメモリに格納する */
  CopyLoadSegments(kernel_ehdr);
  Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

  /* 一時領域を解放する */
  status = gBS->FreePool(kernel_buffer);
  /* 解放に失敗した場合 */
  if (EFI_ERROR(status)) {
    Print(L"failed to free pool: %r\n", status);
    Halt();
  }

  /* ブートサービスを停止させる */
  status = gBS->ExitBootServices(image_handle, memmap.map_key); // ブートサービスの停止指示
  /* ブートサービスの停止に失敗した場合 */
  if (EFI_ERROR(status)) {
    status = GetMemoryMap(&memmap);  // メモリマップ情報を再取得
    /* メモリマップ情報の再取得に失敗した場合 */
    if (EFI_ERROR(status)) {
      Print(L"failed to get memory map: %r\n", status);
      Halt();
    }
    /* ブートサービスを再度停止 */
    status = gBS->ExitBootServices(image_handle, memmap.map_key); // ブートサービスの停止指示
    /* ブートサービスの停止に再度失敗した場合 */
    if (EFI_ERROR(status)) {
      Print(L"Could not exit boot service: %r\n", status);
      Halt();
    }
  }

  /* カーネルファイルを実行する */
  UINT64 entry_addr = *(UINT64*)(kernel_first_addr + 24);  //エントリポイントアドレスを取得する。（最初に実行すべき関数が格納されているアドレス）
  /* 引数でGOPを渡す用の構造体を定義する */
  struct FrameBufferConfig config = {
    (UINT8*)gop->Mode->FrameBufferBase,
    gop->Mode->Info->PixelsPerScanLine,
    gop->Mode->Info->HorizontalResolution,
    gop->Mode->Info->VerticalResolution,
    0
  };
  /* ピクセルのデータ形式を引数用の構造体にセットする */
  switch (gop->Mode->Info->PixelFormat) {
    /* ピクセルのデータ形式がRGBの場合 */
    case PixelRedGreenBlueReserved8BitPerColor:
      config.pixel_format = kPixelRGBResv8BitPerColor;
      break;
    /* ピクセルのデータ形式がBGRの場合 */
    case PixelBlueGreenRedReserved8BitPerColor:
      config.pixel_format = kPixelBGRResv8BitPerColor;
      break;
    /* それ以外の場合 */
    default:
      Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
      Halt();
  }

  typedef void EntryPointType(const struct FrameBufferConfig*);  // エントリポイントを実行する関数の関数ポインタを定義
  EntryPointType* entry_point = (EntryPointType*)entry_addr; // エントリポイントのアドレスをエントリポイントを実行する関数の関数ポインタにキャストする。
  entry_point(&config);  //　エントリポイントを実行

  Print(L"All done\n");

  while (1);
  return EFI_SUCCESS;
}
