/***************************************************************************
		Copyright (C) 2002-2012 Kentaro Kitagawa
		                   kitag@kochi-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "tcp.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
 
#define MIN_BUFFER_SIZE 256

XPosixTCPPort::XPosixTCPPort(XCharInterface *interface)
	: XPort(interface), m_socket(-1) {

}
XPosixTCPPort::~XPosixTCPPort() {
    if(m_socket >= 0) close(m_socket);
}
void
XPosixTCPPort::open() throw (XInterface::XCommError &) {
	Snapshot shot( *m_pInterface);

	struct sockaddr_in dstaddr;

	std::string ipaddr = shot[ *m_pInterface->port()];
	int colpos = ipaddr.find_first_of(':');
	if(colpos == std::string::npos)
		throw XInterface::XCommError(i18n("tcp socket creation failed"), __FILE__, __LINE__);
	unsigned int port;
	if(sscanf(ipaddr.substr(colpos + 1).c_str(), "%u", &port) != 1)
		throw XInterface::XCommError(i18n("tcp socket creation failed"), __FILE__, __LINE__);
	ipaddr = ipaddr.substr(0, colpos);

	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(m_socket < 0) {
		throw XInterface::XCommError(i18n("tcp socket creation failed"), __FILE__, __LINE__);
	}

	struct timeval timeout;
	timeout.tv_sec  = 3;
	timeout.tv_usec = 0;
	if(setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO,  (char*)&timeout, sizeof(timeout)) ||
		setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO,  (char*)&timeout, sizeof(timeout))){
		throw XInterface::XCommError(i18n("tcp socket creation failed"), __FILE__, __LINE__);
	}

	memset( &dstaddr, 0, sizeof(dstaddr));
	dstaddr.sin_port = htons(port);
	dstaddr.sin_family = AF_INET;
	dstaddr.sin_addr.s_addr = inet_addr(ipaddr.c_str());

	if(connect(m_socket, (struct sockaddr *) &dstaddr, sizeof(dstaddr)) == -1) {
		throw XInterface::XCommError(i18n("tcp open failed"), __FILE__, __LINE__);
	}
}
void
XPosixTCPPort::send(const char *str) throw (XInterface::XCommError &) {
    XString buf(str);
    buf += m_pInterface->eos();
    this->write(buf.c_str(), buf.length());
}
void
XPosixTCPPort::write(const char *sendbuf, int size) throw (XInterface::XCommError &) {
	assert(m_pInterface->isOpened());

	int wlen = 0;
	do {
        int ret = ::write(m_socket, sendbuf, size - wlen);
        if(ret <= 0) {
			throw XInterface::XCommError(i18n("write error"), __FILE__, __LINE__);
        }
        wlen += ret;
        sendbuf += ret;
	} while (wlen < size);
}
void
XPosixTCPPort::receive() throw (XInterface::XCommError &) {
	assert(m_pInterface->isOpened());
    
	buffer().resize(MIN_BUFFER_SIZE);
   
	const char *eos = m_pInterface->eos().c_str();
	unsigned int eos_len = m_pInterface->eos().length();
	unsigned int len = 0;
	for(;;) {
		if(buffer().size() <= len + 1) 
			buffer().resize(len + MIN_BUFFER_SIZE);
		int rlen = ::read(m_socket, &buffer().at(len), 1);
		if(rlen == 0) {
			buffer().at(len) = '\0';
			throw XInterface::XCommError(i18n("read time-out, buf=;") + &buffer().at(0), __FILE__, __LINE__);
		}
		if(rlen < 0) {
			throw XInterface::XCommError(i18n("read error"), __FILE__, __LINE__);
		}
		len += rlen;
		if(len >= eos_len) {
			if( !strncmp(&buffer().at(len - eos_len), eos, eos_len)) {
				break;
			}
		}
	}
    
	buffer().resize(len + 1);
	buffer().at(len) = '\0';
}
void
XPosixTCPPort::receive(unsigned int length) throw (XInterface::XCommError &) {
	assert(m_pInterface->isOpened());
   
	buffer().resize(length);
	unsigned int len = 0;
   
	while(len < length) {
		int rlen = ::read(m_socket, &buffer().at(len), 1);
		if(rlen == 0)
			throw XInterface::XCommError(i18n("read time-out"), __FILE__, __LINE__);
		if(rlen < 0) {
			throw XInterface::XCommError(i18n("read error"), __FILE__, __LINE__);
		}
		len += rlen;
	}
}    