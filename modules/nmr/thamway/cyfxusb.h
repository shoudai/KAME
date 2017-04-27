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
#include "chardevicedriver.h"
#include "charinterface.h"
#include <vector>


struct CyFXUSBDevice {
    CyFXUSBDevice() = delete;
    virtual ~CyFXUSBDevice() = default;

    using List = std::vector<shared_ptr<CyFXUSBDevice>>;
    static List enumerateDevices();

    virtual void open() = 0;
    virtual void close() = 0;

    virtual int initialize() = 0;
    virtual void finalize() = 0;

    void halt();
    void run();
    XString getString(int descid);

    void downloadFX2(const uint8_t* image, int len);

    int bulkWrite(int pipe, const uint8_t *buf, int len);
    int bulkRead(int pipe, uint8_t* buf, int len);

    enum class CtrlReq : uint8_t  {
        USB_REQUEST_GET_STATUS = 0x00, USB_REQUEST_CLEAR_FEATURE = 0x01, USB_REQUEST_SET_FEATURE = 0x03, USB_REQUEST_SET_ADDRESS = 0x05,
        USB_REQUEST_GET_DESCRIPTOR = 0x06, USB_REQUEST_SET_DESCRIPTOR = 0x07, USB_REQUEST_GET_CONFIGURATION = 0x08, USB_REQUEST_SET_CONFIGURATION = 0x09,
        USB_REQUEST_GET_INTERFACE = 0x0A, USB_REQUEST_SET_INTERFACE = 0x0B, USB_REQUEST_SYNCH_FRAME = 0x0C, USB_REQUEST_SET_SEL = 0x30,
        USB_SET_ISOCH_DELAY = 0x31
    };
    enum class CtrlReqType : uint8_t {
        USB_REQUEST_TYPE_STANDARD = (0x00 << 5),
        USB_REQUEST_TYPE_CLASS = (0x01 << 5),
        USB_REQUEST_TYPE_VENDOR = (0x02 << 5),
        USB_REQUEST_TYPE_RESERVED = (0x03 << 5),
        USB_RECIPIENT_DEVICE = 0x00,
        USB_RECIPIENT_INTERFACE = 0x01,
        USB_RECIPIENT_ENDPOINT = 0x02,
        USB_RECIPIENT_OTHER = 0x03 };
    virtual int controlWrite(CtrlReq request, CtrlReqType type, uint16_t value,
                             uint16_t index, const uint8_t *buf, int len) = 0;
    virtual int controlRead(CtrlReq request, CtrlReqType type, uint16_t value,
                            uint16_t index, uint8_t *buf, int len) = 0;

    unsigned int vendorID() const {return m_vendorID;}
    unsigned int productID() const {return m_productID;}

    struct AsyncIO {
        class Transfer;
        AsyncIO(unique_ptr<Transfer>&& t);
        AsyncIO(const AsyncIO&) = delete;
        AsyncIO(AsyncIO &&) = default;
        void finalize(int64_t count_imm) {
            m_count_imm = count_imm;
        }
        bool hasFinished() const;
        int64_t waitFor();
        Transfer *ptr() {return m_transfer.get();}
    private:
        unique_ptr<Transfer> m_transfer;
        int64_t m_count_imm = -1;
    };
    virtual AsyncIO asyncBulkWrite(int pipe, const uint8_t *buf, int len) = 0;
    virtual AsyncIO asyncBulkRead(int pipe, uint8_t *buf, int len) = 0;

    XRecursiveMutex mutex;
    XString label;

    enum {USB_DEVICE_DESCRIPTOR_TYPE = 1, USB_CONFIGURATION_DESCRIPTOR_TYPE = 2,
        USB_STRING_DESCRIPTOR_TYPE = 3, USB_INTERFACE_DESCRIPTOR_TYPE = 4,
        USB_ENDPOINT_DESCRIPTOR_TYPE = 5};
//    //AE18AA60-7F6A-11d4-97DD-00010229B959
//    static const std::initializer_list<uint32_t> CYPRESS_GUID = {0xae18aa60, 0x7f6a, 0x11d4, 0x97, 0xdd, 0x0, 0x1, 0x2, 0x29, 0xb9, 0x59};
protected:
    uint16_t m_vendorID, m_productID;
};

//! interfaces Cypress FX2LP/FX3 devices
template <class USBDevice = CyFXUSBDevice>
class XCyFXUSBInterface : public XCustomCharInterface {
public:
    XCyFXUSBInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver);
    virtual ~XCyFXUSBInterface();

    virtual void open() throw (XInterfaceError &);
    //! This can be called even if has already closed.
    virtual void close() throw (XInterfaceError &);

    void lock() {m_usbDevice->mutex->lock();} //!<overrides XInterface::lock().
    void unlock() {m_usbDevice->mutex->unlock();}
    bool isLocked() const {return m_usbDevice->isLockedByCurrentThread();}

    virtual void send(const char *) throw (XCommError &) override {}
    virtual void receive() throw (XCommError &) override {}

    virtual bool isOpened() const override {return usb();}
protected:
    //\return true if device is supported by this interface.
    virtual bool examineDeviceBeforeFWLoad(const USBDevice &dev) = 0;
    //\return device string to be shown in the list box, if it is supported.
    virtual std::string examineDeviceAfterFWLoad(const USBDevice &dev) = 0;
    //\return Relative path to the GPIB wave file.
    virtual XString gpifWave() = 0;
    //\return Relative path to the firmware file.
    virtual XString firmware() = 0;

    const shared_ptr<USBDevice> &usb() const {return m_usbDevice;}
private:
    shared_ptr<USBDevice> m_usbDevice;
    static XMutex s_mutex;
    static typename USBDevice::List s_devices;
    static int s_refcnt;
    static void openAllEZUSBdevices();
    static void setWave(void *handle, const uint8_t *wave);
    static void closeAllEZUSBdevices();
};

