#ifndef __LINUX_WINDEF_H__
#define __LINUX_WINDEF_H__

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <netinet/in.h>

#define _T(x) x
typedef char TCHAR;
typedef const TCHAR *LPCTSTR;

#define _W64 __attribute__((mode (__pointer__)))

typedef uint8_t byte;

typedef int64_t __int64;

typedef int BOOLEAN;
typedef int BOOL;
typedef int INT;
typedef unsigned int UINT;
typedef long long REFERENCE_TIME;

typedef uint8_t BYTE, *PBYTE;
typedef char CHAR, *PCHAR;
typedef unsigned char UCHAR, *PUCHAR;
typedef short SHORT, *PSHORT;
typedef unsigned short USHORT, *PUSHORT;
typedef unsigned short WORD, *PWORD;
typedef long LONG, *PLONG;
typedef unsigned long ULONG, *PULONG;
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
typedef unsigned int UINT;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;

typedef wchar_t WCHAR, *PWCHAR, *WSTRING;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;

typedef void VOID, *PVOID;
typedef PVOID HWND;
typedef PVOID HANDLE;

typedef _W64 uintptr_t ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;

#define FALSE 0U
#define TRUE 1U

#define INFINITE 0xFFFFFFFF

typedef struct _GUID {
  DWORD Data1;
  WORD  Data2;
  WORD  Data3;
  BYTE  Data4[8];
} GUID;

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
#define INVALID_SOCKET -1

typedef int32_t NTSTATUS;
#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L) // ntsubauth
#define STATUS_IO_DEVICE_ERROR           ((NTSTATUS)0xC0000185L)
#define STATUS_UNSUCCESSFUL       ((NTSTATUS)0xC0000001L)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

typedef enum _DEVICE_POWER_STATE {
  PowerDeviceUnspecified  = 0,
  PowerDeviceD0           = 1,
  PowerDeviceD1           = 2,
  PowerDeviceD2           = 3,
  PowerDeviceD3           = 4,
  PowerDeviceMaximum      = 5
} DEVICE_POWER_STATE, *PDEVICE_POWER_STATE;

typedef enum  {
  KS_AnalogVideo_None         = 0x00000000,
  KS_AnalogVideo_NTSC_M       = 0x00000001,
  KS_AnalogVideo_NTSC_M_J     = 0x00000002,
  KS_AnalogVideo_NTSC_433     = 0x00000004,
  KS_AnalogVideo_PAL_B        = 0x00000010,
  KS_AnalogVideo_PAL_D        = 0x00000020,
  KS_AnalogVideo_PAL_G        = 0x00000040,
  KS_AnalogVideo_PAL_H        = 0x00000080,
  KS_AnalogVideo_PAL_I        = 0x00000100,
  KS_AnalogVideo_PAL_M        = 0x00000200,
  KS_AnalogVideo_PAL_N        = 0x00000400,
  KS_AnalogVideo_PAL_60       = 0x00000800,
  KS_AnalogVideo_SECAM_B      = 0x00001000,
  KS_AnalogVideo_SECAM_D      = 0x00002000,
  KS_AnalogVideo_SECAM_G      = 0x00004000,
  KS_AnalogVideo_SECAM_H      = 0x00008000,
  KS_AnalogVideo_SECAM_K      = 0x00010000,
  KS_AnalogVideo_SECAM_K1     = 0x00020000,
  KS_AnalogVideo_SECAM_L      = 0x00040000,
  KS_AnalogVideo_SECAM_L1     = 0x00080000,
  KS_AnalogVideo_PAL_N_COMBO  = 0x00100000
} KS_AnalogVideoStandard;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define _countof(x)         (sizeof(x)/sizeof(x[0]))
#define RtlCopyMemory(dst, src, len) memcpy((void*)(dst), (const void*)(src), (size_t)(len))

#define DELAY_100NS(x)      usleep(x / 10);

typedef LONG HRESULT;
#define S_OK       0x00000000
#define E_FAIL     0x80004005
#define E_POINTER  0x80004003

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

typedef pthread_mutex_t CRITICAL_SECTION;

inline void InitializeCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_init(cs, NULL); }
inline void DeleteCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_destroy(cs); }
inline void EnterCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_lock(cs); }
inline void LeaveCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_unlock(cs); }

inline void closesocket(SOCKET s) { close(s); }
inline void Sleep(DWORD time) { usleep((useconds_t)time * 1000); }

#define stricmp strcasecmp
#define strnicmp strncasecmp

#define _stricmp strcasecmp
#define _strnicmp strncasecmp

#pragma pack(push)
#pragma pack(1)

typedef struct tagBITMAPFILEHEADER {
  uint16_t  bfType;
  uint32_t bfSize;
  uint16_t  bfReserved1;
  uint16_t  bfReserved2;
  uint32_t bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
  uint32_t biSize;
  uint32_t  biWidth;
  uint32_t  biHeight;
  uint16_t  biPlanes;
  uint16_t  biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t  biXPelsPerMeter;
  uint32_t  biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

#pragma pack(pop)

#endif // __LINUX_WINDEF_H__
