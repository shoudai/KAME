/*** FX2FW Ver1.00 by OPTIMIZE ***/
//#include "c:\cypress\usb\drivers\ezusbdrv\ezusbsys.h"
#define IOCTL_Ezusb_GET_STRING_DESCRIPTOR 0x222044
#define IOCTL_Ezusb_ANCHOR_DOWNLOAD 0x22201c
#define IOCTL_Ezusb_VENDOR_REQUEST 0x222014
#define IOCTL_EZUSB_BULK_WRITE 0x222051
#define IOCTL_EZUSB_BULK_READ 0x22204e
#define IOCTL_Ezusb_RESETPIPE 0x222035

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short int u16;
typedef signed short int s16;
typedef unsigned int u32;
typedef signed int s32;

#define CUSB_DEBUG   0
#define	CUSB_DWLSIZE 0x2000

s32 usb_open(s32 n,HANDLE *h);
s32 usb_close(HANDLE *h);
s32 usb_halt(HANDLE *h);
s32 usb_run(HANDLE *h);
s32 usb_dwnload(HANDLE *h,u8 *image,s32 len);
s32 usb_resetpipe(HANDLE *h,ULONG p);
s32 usb_bulk_write(HANDLE *h,s32 pipe,u8 *buf,s32 len);
s32 usb_bulk_read(HANDLE *h,s32 pipe,u8 *buf,s32 len);
s32 cusb_init(s32 n,HANDLE *h,u8 *fw,s8 *str1,s8 *str2);
