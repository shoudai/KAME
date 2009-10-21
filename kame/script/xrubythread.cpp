/***************************************************************************
		Copyright (C) 2002-2009 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "xrubythread.h"

XRubyThread::XRubyThread(const char *name, bool runtime, const XString &filename)
	: XNode(name, runtime),
	  m_filename(create<XStringNode>("Filename", true)),
	  m_status(create<XStringNode>("Status", true)),
	  m_action(create<XStringNode>("Action", true)),
	  m_threadID(create<XIntNode>("ThreadID", true)),
	  m_lineinput(create<XStringNode>("LineInput", true))
{
    m_threadID->value(-1);
    m_filename->value(filename);
    m_action->value(RUBY_THREAD_ACTION_STARTING);
    m_status->value(RUBY_THREAD_STATUS_STARTING);
    lineinput()->setUIEnabled(false);
    m_lsnOnLineChanged = lineinput()->onValueChanged().connectWeak(shared_from_this(),
        &XRubyThread::onLineChanged);
}
 
bool
XRubyThread::isRunning() const
{
    return (XString(*m_status) == RUBY_THREAD_STATUS_RUN);
}
bool
XRubyThread::isAlive() const
{
    return (XString(*m_status) != RUBY_THREAD_STATUS_N_A);
}
void
XRubyThread::kill()
{
    m_action->value(RUBY_THREAD_ACTION_KILL);
	lineinput()->setUIEnabled(false);
}
void
XRubyThread::resume()
{
    m_action->value(RUBY_THREAD_ACTION_WAKEUP);
}
void
XRubyThread::onLineChanged(const shared_ptr<XValueNodeBase> &)
{
	XString line = *lineinput();
	XScopedLock<XMutex> lock(m_lineBufferMutex);
	m_lineBuffer.push_back(line);
	lineinput()->onValueChanged().mask();
	lineinput()->value("");
	lineinput()->onValueChanged().unmask();
}

XString
XRubyThread::gets() {	
	XScopedLock<XMutex> lock(m_lineBufferMutex);
	if(!m_lineBuffer.size()) {
		lineinput()->setUIEnabled(true);
		return XString();
	}
	XString line = m_lineBuffer.front();
	m_lineBuffer.pop_front();
//	lineinput()->setUIEnabled(false);
	return line + "\n";
}
