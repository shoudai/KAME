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
#ifndef GPIB_H_
#define GPIB_H_

#include "charinterface.h"

#ifdef HAVE_LINUX_GPIB
#define GPIB_NI
#endif

#ifdef HAVE_NI488
#define GPIB_NI
#endif

#if defined GPIB_WIN32_IF_4304 || defined GPIB_NI
#define USE_GPIB
#endif

#ifdef GPIB_NI

class XNIGPIBPort : public XPort
{
public:
	XNIGPIBPort(XCharInterface *interface);
	virtual ~XNIGPIBPort();
 
	virtual void open() throw (XInterface::XCommError &);
	virtual void send(const char *str) throw (XInterface::XCommError &);
	virtual void write(const char *sendbuf, int size) throw (XInterface::XCommError &);
	virtual void receive() throw (XInterface::XCommError &);
	virtual void receive(unsigned int length) throw (XInterface::XCommError &);

private:
	int m_ud;
	void gpib_close() throw (XInterface::XCommError &);
	//! reopen device
	void gpib_reset() throw (XInterface::XCommError &);
	void gpib_spoll_before_write() throw (XInterface::XCommError &);
	void gpib_spoll_before_read() throw (XInterface::XCommError &);
	XString gpibStatus(const XString &msg);
	unsigned int gpib_receive(unsigned int est_length, unsigned int max_length)
		throw (XInterface::XCommError &);
	static int s_cntOpened;
	static XMutex s_lock;
};

typedef XNIGPIBPort XGPIBPort;

#endif /*GPIB_NI*/

#endif /*GPIB_H_*/
