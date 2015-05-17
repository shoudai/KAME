/***************************************************************************
		Copyright (C) 2002-2015 Kentaro Kitagawa
		                   kitagawa@phys.s.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef CHARINTERFACE_H_
#define CHARINTERFACE_H_

#include "interface.h"

//#include <stdarg.h>

class DECLSPEC_SHARED XCustomCharInterface : public XInterface {
public:
    XCustomCharInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver);
    virtual ~XCustomCharInterface() {}

    //! Buffer is Thread-Local-Strage.
    //! Therefore, be careful when you access multi-interfaces in one thread.
    //! \sa XThreadLocal
    const std::vector<char> &buffer() const {return *s_tlBuffer;}
    //! error-check is user's responsibility.
    int scanf(const char *format, ...) const
#if defined __GNUC__ || defined __clang__
        __attribute__ ((format(scanf,2,3)))
#endif
    ;
    double toDouble() const throw (XConvError &);
    int toInt() const throw (XConvError &);
    unsigned int toUInt() const throw (XConvError &);
    XString toStr() const;
    XString toStrSimplified() const; //!< returns string white-spaces stripped.

    //! format version of send()
    //! \sa printf()
    void sendf(const char *format, ...) throw (XInterfaceError &)
#if defined __GNUC__ || defined __clang__
        __attribute__ ((format(printf,2,3)))
#endif
    ;
    //! format version of query()
    //! \sa printf()
    void queryf(const char *format, ...) throw (XInterfaceError &)
#if defined __GNUC__ || defined __clang__
        __attribute__ ((format(printf,2,3)))
#endif
    ;

    void setEOS(const char *str);
    const XString &eos() const {return m_eos;}

    void query(const XString &str) throw (XCommError &);
    virtual void query(const char *str) throw (XCommError &);
    virtual void send(const char *str) throw (XCommError &) = 0;
    virtual void receive() throw (XCommError &) = 0;

    virtual bool isOpened() const = 0;

    //! only for XPort and internal use.
    std::vector<char> &buffer_receive() const {return *s_tlBuffer;}
protected:
    virtual void open() throw (XInterfaceError &) = 0;
    //! This can be called even if has already closed.
    virtual void close() throw (XInterfaceError &) = 0;

private:
    static XThreadLocal<std::vector<char> > s_tlBuffer;
    XString m_eos;
};

class XPort;
//! Standard interface for character devices. e.g. GPIB, serial port, TCP/IP...
class DECLSPEC_SHARED XCharInterface : public XCustomCharInterface {
public:
	XCharInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver);
	virtual ~XCharInterface() {}

	void setGPIBUseSerialPollOnWrite(bool x) {m_bGPIBUseSerialPollOnWrite = x;}
	void setGPIBUseSerialPollOnRead(bool x) {m_bGPIBUseSerialPollOnRead = x;}
	void setGPIBWaitBeforeWrite(int msec) {m_gpibWaitBeforeWrite = msec;}
	void setGPIBWaitBeforeRead(int msec) {m_gpibWaitBeforeRead = msec;}
	void setGPIBWaitBeforeSPoll(int msec) {m_gpibWaitBeforeSPoll = msec;}
	void setGPIBMAVbit(unsigned char x) {m_gpibMAVbit = x;}
  
	bool gpibUseSerialPollOnWrite() const {return m_bGPIBUseSerialPollOnWrite;}
	bool gpibUseSerialPollOnRead() const {return m_bGPIBUseSerialPollOnRead;}
	int gpibWaitBeforeWrite() const {return m_gpibWaitBeforeWrite;}
	int gpibWaitBeforeRead() const {return m_gpibWaitBeforeRead;}
	int gpibWaitBeforeSPoll() const {return m_gpibWaitBeforeSPoll;}
	unsigned char gpibMAVbit() const {return m_gpibMAVbit;}
	
	//! These properties should be set before open().
	void setSerialBaudRate(unsigned int rate) {m_serialBaudRate = rate;}
	void setSerialStopBits(unsigned int bits) {m_serialStopBits = bits;}
	enum {PARITY_NONE = 0, PARITY_ODD = 1, PARITY_EVEN = 2};
	void setSerialParity(unsigned int parity) {m_serialParity = parity;}
	void setSerial7Bits(bool enable) {m_serial7Bits = enable;}
	void setSerialFlushBeforeWrite(bool x) {m_serialFlushBeforeWrite = x;}
	void setSerialEOS(const char *str) {m_serialEOS = str;} //!< be overridden by \a setEOS().
	void setSerialHasEchoBack(bool x) {m_serialHasEchoBack = x;}
  
	unsigned int serialBaudRate() const {return m_serialBaudRate;}
	unsigned int serialStopBits() const {return m_serialStopBits;}
	unsigned int serialParity() const {return m_serialParity;}
	bool serial7Bits() const {return m_serial7Bits;}
	bool serialFlushBeforeWrite() const {return m_serialFlushBeforeWrite;}
	const XString &serialEOS() const {return m_serialEOS;}
	bool serialHasEchoBack() const {return m_serialHasEchoBack;}

    virtual void send(const XString &str) throw (XCommError &);
    virtual void send(const char *str) throw (XCommError &);
    virtual void write(const char *sendbuf, int size) throw (XCommError &);
    virtual void receive() throw (XCommError &);
    virtual void receive(unsigned int length) throw (XCommError &);
	virtual bool isOpened() const {return !!m_xport;}
protected:
	virtual void open() throw (XInterfaceError &);
	//! This can be called even if has already closed.
	virtual void close() throw (XInterfaceError &);
private:
    XString m_serialEOS;
	bool m_bGPIBUseSerialPollOnWrite;
	bool m_bGPIBUseSerialPollOnRead;
	int m_gpibWaitBeforeWrite;
	int m_gpibWaitBeforeRead;
	int m_gpibWaitBeforeSPoll;
	unsigned char m_gpibMAVbit; //! don't check if zero
  
	unsigned int m_serialBaudRate;
	unsigned int m_serialStopBits;
	unsigned int m_serialParity;
	bool m_serial7Bits;
	bool m_serialFlushBeforeWrite;
	bool m_serialHasEchoBack;
	
	shared_ptr<XPort> m_xport;

	//! for scripting
	shared_ptr<XStringNode> m_script_send;
	shared_ptr<XStringNode> m_script_query;
	shared_ptr<XListener> m_lsnOnSendRequested;
	shared_ptr<XListener> m_lsnOnQueryRequested;
	void onSendRequested(const Snapshot &shot, XValueNodeBase *);
	void onQueryRequested(const Snapshot &shot, XValueNodeBase *);
};

class XPort {
public:
    XPort(XCharInterface *interface): m_pInterface(interface) {}
    virtual ~XPort() {}
	virtual void open() throw (XInterface::XCommError &) = 0;
	virtual void send(const char *str) throw (XInterface::XCommError &) = 0;
	virtual void write(const char *sendbuf, int size) throw (XInterface::XCommError &) = 0;
	virtual void receive() throw (XInterface::XCommError &) = 0;
	virtual void receive(unsigned int length) throw (XInterface::XCommError &) = 0;
	//! Thread-Local-Storage Buffer.
	//! \sa XThreadLocal
    std::vector<char>& buffer() {return m_pInterface->buffer_receive();}
protected:
	XCharInterface *const m_pInterface;
};

#endif /*CHARINTERFACE_H_*/
