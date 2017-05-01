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

#define NOMINMAX
#include <windows.h>

constexpr uint32_t IOCTL_EZUSB_GET_DEVICE_DESCRIPTOR =  0x222004;
constexpr uint32_t IOCTL_EZUSB_GET_STRING_DESCRIPTOR = 0x222044;
constexpr uint32_t IOCTL_EZUSB_ANCHOR_DOWNLOAD = 0x22201c;
constexpr uint32_t IOCTL_EZUSB_VENDOR_REQUEST = 0x222014;
constexpr uint32_t IOCTL_EZUSB_BULK_WRITE = 0x222051;
constexpr uint32_t IOCTL_EZUSB_BULK_READ = 0x22204e;

constexpr uint32_t IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER = 0x220020;
constexpr uint32_t IOCTL_ADAPT_SEND_NON_EP0_TRANSFER = 0x220024;
constexpr uint32_t IOCTL_ADAPT_SEND_NON_EP0_DIRECT = 0x22004b;

struct CyFXUSBDevice::AsyncIO::Transfer {
    OVERLAPPED overlap;
    HANDLE handle;
    unique_ptr<std::vector<uint8_t>> ioctlbuf;
    void *ioctlbuf_rdpos = nullptr;
    uint8_t *rdbuf = nullptr;
};

CyFXUSBDevice::AsyncIO::AsyncIO() : m_transfer(new Transfer) {
}
bool
CyFXUSBDevice::AsyncIO::hasFinished() {
    return HasOverlappedIoCompleted( &ptr()->overlap);
}
int64_t
CyFXUSBDevice::AsyncIO::waitFor() {
    if( !m_count_imm) {
        DWORD num;
        if( !GetOverlappedResult(ptr()->handle, &ptr()->overlap, &num, true)) {
            auto e = GetLastError();
            throw XInterfaceError("Error during USB tranfer:%d.", (int)e);
        }
        finalize(num);
    }
    if(ptr()->rdbuf) {
        std::copy(ptr()->ioctlbuf_rdpos, &ptr()->ioctlbuf->at(0) + m_count_num, ptr()->rdbuf);
    }
    return m_count_imm;
}
CyFXUSBDevice::AsyncIO
CyFXWin32USBDevice::async_ioctl(uint64_t code, const void *in, ssize_t size_in, void *out, ssize_t size_out) {
    DWORD nbyte;
    AsyncIO async(tr);
    CyFXUSBDevice::AsyncIO::Transfer& tr = *async.ptr();
    tr.handle = handle;
    if( !DeviceIoControl(handle, code, in, size_in, out, size_out, &nbyte, &async.ptr()->tr)) {
        auto e = GetLastError();
        if(e == ERROR_IO_PENDING)
            return std::move(async);
        throw XInterfaceError("IOCTL error:%d.", (int)e);
    }
    async.finanlize(nbyte);
    return std::move(async);
}

CyFXWin32USBDevice::CyFXWin32USBDevice(HANDLE h, const XString &n) : handle(h), name(n) {
    //obtains device descriptor
    DeviceDescriptor dev_desc;
    auto buf = reinterpret_cast<uint8_t*>( &dev_desc);
    //Reads common descriptor.
    controlRead(CtrlReq::USB_REQUEST_GET_DESCRIPTOR,
        CtrlReqType::STD, USB_DEVICE_DESCRIPTOR_TYPE * 0x100u,
        0, buf, sizeof(dev_desc));

    m_productID = dev_desc.idProduct;
    m_vendorID = dev_desc.idVendor;
}


int64_t
CyFXWin32USBDevice::ioctl(uint64_t code, const void *in, ssize_t size_in, void *out, ssize_t size_out) {
    auto async = async_ioctl(code, in, size_in, out, size_out);
    return async.waitFor();
}

CyFXUSBDevice::List
CyFXUSBDevice::enumerateDevices() {
    CyFXUSBDevice::List list;
    //Searching for ezusb.sys devices.
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
        list.push_back(std::make_shared<CyFXEzUSBDevice>(h, name));
    }
    //CyUSB3.sys
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

void
CyUSB3Device::finalize() {

}


XString
CyFXEzUSBDevice::getString(int descid) {
    uint8_t  buf[2] = {};
    StringDescCtrl sin;
    sin.Index = descid;
    sin.LanguageId = 27;
    //Reads common descriptor.
    ioctl(IOCTL_EZUSB_GET_STRING_DESCRIPTOR, &sin, sizeof(sin), buf, sizeof(buf));

    int len = buf[0];
    {
        //Reads string descriptor.
        uint8_t buf[len] = {};
        char str[len / 2 + 1] = {};
        int ret = ioctl(IOCTL_EZUSB_GET_STRING_DESCRIPTOR, &sin, sizeof(sin), buf, sizeof(buf));
        if(ret <= 2)
            throw XInterfaceError(i18n("Size mismatch during control transfer."));
        uint8_t desc_type = buf[1];
        if(desc_type != USB_STRING_DESCRIPTOR_TYPE)
            throw XInterfaceError(i18n("Size mismatch during control transfer."));
        for(int i = 0; i < buf[0]/2 - 1; i++){
            *(str++) = (char)buf[2 * i + 2];
        }
        return {str};
    }
    return {str};
}

CyFXUSBDevice::AsyncIO
CyFXEzUSBDevice::asyncBulkWrite(uint8_t ep, const uint8_t *buf, int len) {
    unique_ptr<std::vector<uint8_t>> ioctlbuf(new std::vector<uint8_t>(sizeof(BulkTransferCtrl)));
    auto tr = reiterpret_cast<BulkTransferCtrl *>(&ioctlbuf->at(0));
    //FX2FW specific
    switch(ep) {
    case 2:
        tr.pipeNum = 0;
        break;
    case 8:
        tr.pipeNum = 1;
        break;
    default:
        throw XInterface::XInterfaceError("Unknown pipe", __FILE__, __LINE__);
    }

    auto ret = async_ioctl(IOCTL_EZUSB_BULK_WRITE,
        &ioctlbuf->at(0), ioctlbuf->size(), &buf, l);
    ret.ioctlbuf = std::move(ioctlbuf);
    return std::move(ret);
}

CyFXUSBDevice::AsyncIO
CyFXEzUSBDevice::asyncBulkRead(uint8_t ep, uint8_t* buf, int len) {
    unique_ptr<std::vector<uint8_t>> ioctlbuf(new std::vector<uint8_t>(sizeof(BulkTransferCtrl)));
    auto tr = reiterpret_cast<BulkTransferCtrl *>(&ioctlbuf->at(0));
    //FX2FW specific
    switch(ep) {
    case 6:
        tr.pipeNum = 2;
        break;
    default:
        throw XInterface::XInterfaceError("Unknown pipe", __FILE__, __LINE__);
    }

    auto ret = async_ioctl(IOCTL_EZUSB_BULK_READ,
        &ioctlbuf->at(0), ioctlbuf->size(), &buf, l);
    ret.ioctlbuf = std::move(ioctlbuf);
    return std::move(ret);
}


int
CyFXEzUSBDevice::controlWrite(CtrlReq request, CtrlReqType type, uint16_t value,
                               uint16_t index, const uint8_t *wbuf, int len) {
    if(type == CtrlReqType::USB_REQUEST_TYPE_VENDOR) {
        uint8_t buf[sizeof(VendorRequestCtrl) + len];
        auto tr = reiterpret_cast<VendorRequestCtrl *>(buf);
        *tr = VendorRequestCtrl{}; //0 fill.
        vreq.bRequest = request;
        vreq.wValue = value;
        vreq.wIndex = index;
        vreq.wLength = len;
        vreq.direction = 0;
        return ioctl(IOCTL_EZUSB_VENDOR_REQUEST, &buf, sizeof(buf), NULL, 0);
    }
}

int
CyFXEzUSBDevice::controlRead(CtrlReq request, CtrlReqType type, uint16_t value,
                               uint16_t index, uint8_t *rdbuf, int len) {
    switch(type) {
    case CtrlReqType::USB_REQUEST_TYPE_VENDOR:
        uint8_t buf[sizeof(VendorRequestCtrl)];
        auto tr = reiterpret_cast<VendorRequestCtrl *>(buf);
        *tr = VendorRequestCtrl{}; //0 fill.
        vreq.bRequest = request;
        vreq.wValue = value;
        vreq.wIndex = index;
        vreq.wLength = len;
        vreq.direction = 1;
        return ioctl(IOCTL_EZUSB_VENDOR_REQUEST, &buf, sizeof(buf), rdbuf, len);
    case CtrlReqType::USB_REQUEST_TYPE_STANDARD:
        if(request == CtrlReq::USB_REQUEST_GET_DESCRIPTOR) {
            if(value / 0x100u == USB_DEVICE_DESCRIPTOR_TYPE) {
                return ioctl(IOCTL_EZUSB_GET_DEVICE_DESCRIPTOR, NULL, 0, rdbuf, len);
            }
        }
    }
}

int
CyUSB3Device::controlWrite(CtrlReq request, CtrlReqType type, uint16_t value,
                               uint16_t index, const uint8_t *wbuf, int len) {
    uint8_t buf[sizeof(SingleTransfer) + len];
    auto tr = reiterpret_cast<SingleTransfer *>(buf);
    *tr = SingleTransfer{}; //0 fill.
    std::copy(wbuf, wbuf + len, &buf[sizeof(SingleTransfer)]);
    tr.bmRequest = type;
    tr.request = request;
    tr.value = value;
    tr.index = index;
    tr.length = len;
    tr.timeOut = 1; //sec?
    tr.endpointAddress = 0;
    tr.bufferOffset = sizeof(SingleTransfer);
    tr.bufferLength = length;
    return ioctl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, &buf, sizeof(buf), &buf, sizeof(buf));
}

int
CyUSB3Device::controlRead(CtrlReq request, CtrlReqType type, uint16_t value,
                               uint16_t index, uint8_t *rdbuf, int len) {
    uint8_t buf[sizeof(SingleTransfer) + len];
    auto tr = reiterpret_cast<SingleTransfer *>(buf);
    *tr = SingleTransfer{}; //0 fill.
    tr.bmRequest = 0x80u | type;
    tr.request = request;
    tr.value = value;
    tr.index = index;
    tr.length = len;
    tr.timeOut = 1; //sec?
    tr.endpointAddress = 0;
    tr.bufferOffset = sizeof(SingleTransfer);
    tr.bufferLength = length;
    int ret = ioctl(IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, &buf, sizeof(buf), &buf, sizeof(buf));
    if((ret < sizeof(SingleTransfer)) || (ret > sizeof(SingleTransfer) + len))
        throw XInterfaceError(i18n("Size mismatch during control transfer."));
    std::copy(buf + sizeof(SingleTransfer), buf + ret, rdbuf);
    return ret;
}


XString
CyUSB3Device::getString(int descid) {
    uint8_t buf[2];
    //Reads common descriptor.
    controlRead(CtrlReq::GET_DESC,
        CtrlReqType::STD, USB_STRING_DESCRIPTOR_TYPE * 0x100u + descid,
        27, buf, sizeof(buf));
    int len = buf[0];
    {
        //Reads string descriptor.
        uint8_t buf[len] = {};
        char str[len / 2 + 1] = {};
        int ret = controlRead(CtrlReq::GET_DESC,
            CtrlReqType::STD, USB_STRING_DESCRIPTOR_TYPE * 0x100u + descid,
            27, buf, len);
        if(ret <= 2)
            throw XInterfaceError(i18n("Size mismatch during control transfer."));
        uint8_t desc_type = buf[1];
        if(desc_type != USB_STRING_DESCRIPTOR_TYPE)
            throw XInterfaceError(i18n("Size mismatch during control transfer."));
        for(int i = 0; i < buf[0]/2 - 1; i++){
            *(str++) = (char)buf[2 * i + 2];
        }
        return {str};
    }
}

CyFXUSBDevice::AsyncIO
CyUSB3Device::asyncBulkWrite(uint8_t ep, const uint8_t *buf, int len) {
    unique_ptr<std::vector<uint8_t>> ioctlbuf(new std::vector<uint8_t>(sizeof(SingleTransfer) + len));
    auto tr = reiterpret_cast<SingleTransfer *>(&ioctlbuf->at(0));
    *tr = SingleTransfer{}; //0 fill.
    std::copy(buf, buf + len, &ioctlbuf->at(sizeof(SingleTransfer)));
    tr.timeOut = 1; //sec?
    tr.endpointAddress = ep;
    tr.bufferOffset = sizeof(SingleTransfer);
    tr.bufferLength = len;
    auto ret = async_ioctl(IOCTL_ADAPT_SEND_NON_EP0_TRANSFER,
        &ioctlbuf->at(0), ioctlbuf->size(), &ioctlbuf->at(0), ioctlbuf->size());
    ret.ioctlbuf = std::move(ioctlbuf);
    return std::move(ret);
}

CyFXUSBDevice::AsyncIO
CyUSB3Device::asyncBulkRead(uint8_t ep, uint8_t* buf, int len) {
    unique_ptr<std::vector<uint8_t>> ioctlbuf(new std::vector<uint8_t>(sizeof(SingleTransfer) + len));
    auto tr = reiterpret_cast<SingleTransfer *>(&ioctlbuf->at(0));
    *tr = SingleTransfer{}; //0 fill.
    tr.timeOut = 1; //sec?
    tr.endpointAddress = 0x80u | (uint8_t)ep;
    tr.bufferOffset = sizeof(SingleTransfer);
    tr.bufferLength = len;
    auto ret = async_ioctl(IOCTL_ADAPT_SEND_NON_EP0_TRANSFER,
        &ioctlbuf->at(0), ioctlbuf->size(), &ioctlbuf->at(0), ioctlbuf->size());
    ret.ioctlbuf = std::move(ioctlbuf);
    ret.ioctlbuf_rdpos = &ioctlbuf->at(tr.bufferOffset);
    ret.rdbuf = buf;
    return std::move(ret);
}


