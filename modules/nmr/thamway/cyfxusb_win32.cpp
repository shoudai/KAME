/***************************************************************************
        Copyright (C) 2002-2017 Kentaro Kitagawa
                           kitagawa@phys.s.u-tokyo.ac.jp

        This program is free software; you can redistribute it and/or
        modify it under the terms of the GNU Library General Public
        License as published by the Free Software Foundation; either
        version 2 of the License, or (at your option) any later version.

        You should have received a copy of the GNU Library General
        Public License and a list of authors along with this program;
        see the files COPYING and AUTHORS.
***************************************************************************/
#include "cyfxusb_win32.h"

constexpr uint32_t IOCTL_EZUSB_GET_STRING_DESCRIPTOR = 0x222044;
constexpr uint32_t IOCTL_EZUSB_ANCHOR_DOWNLOAD = 0x22201c;
constexpr uint32_t IOCTL_EZUSB_VENDOR_REQUEST = 0x222014;
constexpr uint32_t IOCTL_EZUSB_BULK_WRITE = 0x222051;
constexpr uint32_t IOCTL_EZUSB_BULK_READ = 0x22204e;

constexpr uint32_t IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER = 0x220020;
constexpr uint32_t IOCTL_ADAPT_SEND_NON_EP0_TRANSFER = 0x220024;
constexpr uint32_t IOCTL_ADAPT_SEND_NON_EP0_DIRECT = 0x22004b;

using CyFXUSBDevice::AsyncIO::Transfer = OVERLAPPED;

template <class tHANDLE>
CyFXUSBDevice::AsyncIO::AsyncIO(tHANDLE h) : m_transfer(new OVERLAPPED{}), m_count_imm(-1), handle(h) {
}
bool
CyFXUSBDevice::AsyncIO::hasFinished() {
    return HasOverlappedIoCompleted(ptr());
}
int64_t
CyFXUSBDevice::AsyncIO::waitFor() {
    if(m_count_imm) return m_count_imm;
    DWORD num;
    GetOverlappedResult((HANDLE)handle, ptr(), &num, true);
    finalize(num);
    return num;
}
CyFXUSBDevice::ASyncIO
CyFXWin32USBDevice::async_ioctl(uint64_t code, const void *in, ssize_t size_in, void *out, ssize_t size_out) {
    DWORD nbyte;
    AsyncIO async(handle);
    if( !DeviceIoControl(handle, code, in, size_in, out, size_out, &nbyte, async.ptr())) {
        auto e = GetLastError();
        if(e == ERROR_IO_PENDING)
            return std::move(async);
        throw XInterfaceError("IOCTL error:%s.", e);
    }
    async.finanlize(nbyte);
    return std::move(async);
}
int64_t
CyFXWin32USBDevice::ioctl(uint64_t code, const void *in, ssize_t size_in, void *out, ssize_t size_out) {
    auto async = async_ioctl(code, in, size_in, out, size_out);
    return async.waitFor();
}

CyFXUSBDevice::List
CyFXUSBDevice::enumerateDevices() {
    CyFXUSBDevice::List list;
    for(int idx = 0; idx < 9; ++idx) {
        XString name = formatString("\\\\.\\Ezusb-%d",n);
        fprintf(stderr, "cusb: opening device: %s\n", name.c_str());
        HANDLE h = CreateFileA(name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
           FILE_FLAG_OVERLAPPED,
           NULL);
        if(h == INVALID_HANDLE_VALUE) {
            int e = (int)GetLastError();
            if(e != ERROR_FILE_NOT_FOUND)
                throw XInterfaceError(formatString("INVALID HANDLE %d for %s\n", e, name.c_str()));
            if(idx == 0) break;
            return std::move(list);
        }
        list.push_back(std::make_shared<CyFXEasyUSBDevice>(h, name));
    }

    return std::move(list);
}
void
CyFXUSBDevice::open() {
    if( !handle) {
        handle = CreateFileA(name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);
        if(handle == INVALID_HANDLE_VALUE) {
            int e = (int)GetLastError();
            throw XInterfaceError(formatString("INVALID HANDLE %d for %s\n", e, name.c_str()));
        }
    }
}

void
CyFXUSBDevice::close() {
    if(handle) CloseHandle(handle);
    handle = nullptr;
}

int
CyFXUSBDevice::initialize();
void
CyFXUSBDevice::finalize();

void
CyUSB3Device::vendorRequestIn(uint8_t request, uint16_t value,
                               uint16_t index, uint16_t length, uint8_t data) {
    uint8_t buf[sizeof(SingleTransfer) + 1];
    auto tr = reiterpret_cast<SingleTransfer *>(buf);
    *tr = SingleTransfer{}; //0 fill.
    buf[sizeof(SingleTransfer)] = data;
    tr.recipient = 0; //Device
    tr.type = 2; //Vendor Request
    tr.direction = 1; //IN
    assert(length == 1);
    tr.request = request;
    tr.value = value;
    tr.index = index;
    tr.length = length;
    tr.timeOut = 1; //sec?
    tr.endpointAddress = 0x00;
    tr.bufferOffset = sizeof(tr);
    tr.bufferLength = length;
    ioctl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, &buf, sizeof(buf), &buf, sizeof(buf));
}


XString
CyUSB3Device::getString(int descid) {

}

void
CyUSB3Device::download(uint8_t* image, int len);
int
CyUSB3Device::bulkWrite(int pipe, uint8_t *buf, int len) {
    if (hDevice == INVALID_HANDLE_VALUE) return NULL;
    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + bufLen;
    PUCHAR pXmitBuf = new UCHAR[iXmitBufSize]; ZeroMemory(pXmitBuf, iXmitBufSize);
    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER)pXmitBuf; pTransfer->Reserved = 0;
    pTransfer->ucEndpointAddress = Address; pTransfer->IsoPacketLength = 0;
    pTransfer->BufferOffset = sizeof (SINGLE_TRANSFER); pTransfer->BufferLength = bufLen;
    // Copy buf into pXmitBuf
    UCHAR *ptr = (PUCHAR) pTransfer + pTransfer->BufferOffset; memcpy(ptr, buf, bufLen);
    DWORD dwReturnBytes;
    DeviceIoControl(hDevice, IOCTL_ADAPT_SEND_NON_EP0_TRANSFER,
    pXmitBuf, iXmitBufSize, pXmitBuf, iXmitBufSize, &dwReturnBytes, ov);
    return pXmitBuf;
}

int
CyUSB3Device::bulkRead(int pipe, const uint8_t* buf, int len);

unsigned int
CyUSB3Device::vendorID();
unsigned int
CyUSB3Device::productID();


CyFXUSBDevice::AsyncIO
CyFXUSBDevice::asyncBulkWrite(int pipe, uint8_t *buf, int len);
CyFXUSBDevice::AsyncIO
CyFXUSBDevice::asyncBulkRead(int pipe, const uint8_t *buf, int len);

