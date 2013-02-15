/***************************************************************************
		Copyright (C) 2002-2013 Kentaro Kitagawa
		                   kitag@kochi-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "charinterface.h"
//---------------------------------------------------------------------------
#include "measure.h"
#include "xnodeconnector.h"
#include <string>
#include <stdarg.h>
#include "driver.h"
#include "gpib.h"
#include "serial.h"
#include "tcp.h"
#include "dummyport.h"

//---------------------------------------------------------------------------
#define SNPRINT_BUF_SIZE 128

XThreadLocal<std::vector<char> > XPort::s_tlBuffer;

XCharInterface::XCharInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver) : 
    XInterface(name, runtime, driver), 
    m_bGPIBUseSerialPollOnWrite(true),
    m_bGPIBUseSerialPollOnRead(true),
    m_gpibWaitBeforeWrite(1),
    m_gpibWaitBeforeRead(2),
    m_gpibWaitBeforeSPoll(1),
    m_gpibMAVbit(0x10),
    m_serialBaudRate(9600),
    m_serialStopBits(2),
    m_serialParity(PARITY_NONE),
    m_serial7Bits(false),
    m_serialFlushBeforeWrite(true),
    m_script_send(create<XStringNode>("Send", true)),
    m_script_query(create<XStringNode>("Query", true)) {

	for(Transaction tr( *this);; ++tr) {
	#ifdef USE_GPIB
		tr[ *device()].add("GPIB");
	#endif
		tr[ *device()].add("SERIAL");
		tr[ *device()].add("TCP/IP");
		tr[ *device()].add("DUMMY");
  
		m_lsnOnSendRequested = tr[ *m_script_send].onValueChanged().connectWeakly(
			shared_from_this(), &XCharInterface::onSendRequested);
		m_lsnOnQueryRequested = tr[ *m_script_query].onValueChanged().connectWeakly(
			shared_from_this(), &XCharInterface::onQueryRequested);
		if(tr.commit())
			break;
	}
}
void
XCharInterface::setEOS(const char *str) {
    m_eos = str;
}
         
void
XCharInterface::open() throw (XInterfaceError &) {
	m_xport.reset();
    Snapshot shot( *this);
	{
		shared_ptr<XPort> port;
	#ifdef USE_GPIB
		if(shot[ *device()].to_str() == "GPIB") {
			port.reset(new XGPIBPort(this));
		}
	#endif
		if(shot[ *device()].to_str() == "SERIAL") {
			port.reset(new XSerialPort(this));
		}
		if(shot[ *device()].to_str() == "TCP/IP") {
			port.reset(new XTCPPort(this));
		}
		if(shot[ *device()].to_str() == "DUMMY") {
			port.reset(new XDummyPort(this));
		}
          
		if( !port) {
			throw XOpenInterfaceError(__FILE__, __LINE__);
		}
          
		port->open();
		m_xport.swap(port);
	}
}
void
XCharInterface::close() throw (XInterfaceError &) {
	m_xport.reset();
}
int
XCharInterface::scanf(const char *fmt, ...) const {
	if( !buffer().size())
		throw XConvError(__FILE__, __LINE__);
	bool addednull = false;
	if(buffer().back() != '\0') {
		m_xport->buffer().push_back('\0');
		addednull = true;
	}

	int ret;
	va_list ap;

	va_start(ap, fmt);

	ret = vsscanf( &buffer()[0], fmt, ap);

	va_end(ap);

	if(addednull)
		m_xport->buffer().pop_back();
	return ret;    
}
double
XCharInterface::toDouble() const throw (XConvError &) {
    double x;
    int ret = this->scanf("%lf", &x);
    if(ret != 1)
		throw XConvError(__FILE__, __LINE__);
    return x;
}
int
XCharInterface::toInt() const throw (XConvError &) {
    int x;
    int ret = this->scanf("%d", &x);
    if(ret != 1)
		throw XConvError(__FILE__, __LINE__);
    return x;
}
unsigned int
XCharInterface::toUInt() const throw (XConvError &) {
    unsigned int x;
    int ret = this->scanf("%u", &x);
    if(ret != 1)
		throw XConvError(__FILE__, __LINE__);
    return x;
}

XString
XCharInterface::toStr() const {
	return XString( &buffer()[0]);
}
XString
XCharInterface::toStrSimplified() const {
	return QString( &interface()->buffer()[0]).simplified();
}

const std::vector<char> &
XCharInterface::buffer() const {return m_xport->buffer();}

void
XCharInterface::send(const XString &str) throw (XCommError &) {
    this->send(str.c_str());
}
void
XCharInterface::send(const char *str) throw (XCommError &) {
	XScopedLock<XCharInterface> lock(*this);
	try {
		dbgPrint(driver()->getLabel() + " Sending:\"" + dumpCString(str) + "\"");
		m_xport->send(str);
	}
	catch (XCommError &e) {
		e.print(driver()->getLabel() + i18n(" SendError, because "));
		throw e;
	}
}
void
XCharInterface::sendf(const char *fmt, ...) throw (XInterfaceError &) {
	va_list ap;
	int buf_size = SNPRINT_BUF_SIZE;
	std::vector<char> buf;
	for(;;) {
		buf.resize(buf_size);
		int ret;
    
		va_start(ap, fmt);
    
		ret = vsnprintf(&buf[0], buf_size, fmt, ap);
    
		va_end(ap);
      
		if(ret < 0) throw XConvError(__FILE__, __LINE__);
		if(ret < buf_size) break;
      
		buf_size *= 2;
	}
  
	this->send(&buf[0]);
}
void
XCharInterface::write(const char *sendbuf, int size) throw (XCommError &) {
	XScopedLock<XCharInterface> lock(*this);
	try {
		dbgPrint(driver()->getLabel() + formatString(" Sending %d bytes", size));
		m_xport->write(sendbuf, size);
	}
	catch (XCommError &e) {
		e.print(driver()->getLabel() + i18n(" SendError, because "));
		throw e;
	}
}
void
XCharInterface::receive() throw (XCommError &) {
	XScopedLock<XCharInterface> lock(*this);
	try {
		dbgPrint(driver()->getLabel() + " Receiving...");
		m_xport->receive();
		assert(buffer().size());
		dbgPrint(driver()->getLabel() + " Received;\"" + 
				 dumpCString((const char*)&buffer()[0]) + "\"");
	}
	catch (XCommError &e) {
        e.print(driver()->getLabel() + i18n(" ReceiveError, because "));
        throw e;
	}
}
void
XCharInterface::receive(unsigned int length) throw (XCommError &) {
	XScopedLock<XCharInterface> lock(*this);
	try {
		dbgPrint(driver()->getLabel() + QString(" Receiving %1 bytes...").arg(length));
		m_xport->receive(length);
		dbgPrint(driver()->getLabel() + QString(" %1 bytes Received.").arg(buffer().size())); 
	}
	catch (XCommError &e) {
		e.print(driver()->getLabel() + i18n(" ReceiveError, because "));
		throw e;
	}
}
void
XCharInterface::query(const XString &str) throw (XCommError &) {
    query(str.c_str());
}
void
XCharInterface::query(const char *str) throw (XCommError &) {
	XScopedLock<XCharInterface> lock(*this);
	send(str);
	receive();
}
void
XCharInterface::queryf(const char *fmt, ...) throw (XInterfaceError &) {
	va_list ap;
	int buf_size = SNPRINT_BUF_SIZE;
	std::vector<char> buf;
	for(;;) {
		buf.resize(buf_size);
		int ret;
    
		va_start(ap, fmt);
    
		ret = vsnprintf(&buf[0], buf_size, fmt, ap);
    
		va_end(ap);
      
		if(ret < 0) throw XConvError(__FILE__, __LINE__);
		if(ret < buf_size) break;
      
		buf_size *= 2;
	}

	this->query(&buf[0]);
}
void
XCharInterface::onSendRequested(const Snapshot &shot, XValueNodeBase *) {
    XScopedLock<XCharInterface> lock( *this);
	if(!isOpened())
		throw XInterfaceError(i18n("Port is not opened."), __FILE__, __LINE__);
    this->send(shot[ *m_script_send].to_str());
}
void
XCharInterface::onQueryRequested(const Snapshot &shot, XValueNodeBase *) {
    XScopedLock<XCharInterface> lock( *this);
    if(!isOpened())
		throw XInterfaceError(i18n("Port is not opened."), __FILE__, __LINE__);
    this->query(shot[ *m_script_query].to_str());
	for(Transaction tr( *this);; ++tr) {
		tr[ *m_script_query] = XString(&buffer()[0]);
		tr.unmark(m_lsnOnQueryRequested);
		if(tr.commit())
			break;
	}
}

XPort::XPort(XCharInterface *interface)
	: m_pInterface(interface) {
}
XPort::~XPort() {
}
