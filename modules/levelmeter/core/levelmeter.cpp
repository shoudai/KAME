/***************************************************************************
		Copyright (C) 2002-2010 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#include "levelmeter.h"
#include "interface.h"
#include "analyzer.h"

XLevelMeter::XLevelMeter(const char *name, bool runtime, 
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
    XPrimaryDriver(name, runtime, ref(tr_meas), meas) {
}
void
XLevelMeter::showForms() {
//! impliment form->show() here
}

void
XLevelMeter::start() {
	m_thread.reset(new XThread<XLevelMeter>(shared_from_this(), &XLevelMeter::execute));
	m_thread->resume();
}
void
XLevelMeter::stop() {
    if(m_thread) m_thread->terminate();
//    m_thread->waitFor();
//  thread must do interface()->close() at the end
}

void
XLevelMeter::analyzeRaw() throw (XRecordError&) {
	for(unsigned int ch = 0; ch < m_entries.size(); ch++) {
	    m_levelRecorded[ch] = pop<double>();
	    m_entries[ch]->value(m_levelRecorded[ch]);
	}
}
void
XLevelMeter::visualize() {
	//! impliment extra codes which do not need write-lock of record
	//! record is read-locked
}

void
XLevelMeter::createChannels(Transaction &tr_meas, const shared_ptr<XMeasure> &meas,
    const char **channel_names) {
  shared_ptr<XScalarEntryList> entries(meas->scalarEntries());
  
  for(int i = 0; channel_names[i]; i++) {
	    shared_ptr<XScalarEntry> entry(create<XScalarEntry>(
	    	channel_names[i], false,
	       dynamic_pointer_cast<XDriver>(shared_from_this()), "%.4g"));
	     m_entries.push_back(entry);
	     entries->insert(tr_meas, entry);
    }
	m_levelRecorded.resize(m_entries.size());
}


void *
XLevelMeter::execute(const atomic<bool> &terminated) {
    while(!terminated) {
		msecsleep(100);
    	
		clearRaw();
		// try/catch exception of communication errors
		try {
			unsigned int num = m_entries.size();
			for(unsigned int ch = 0; ch < num; ch++)
				push((double)getLevel(ch));
		}
		catch (XKameError &e) {
			e.print(getLabel());
			continue;
		}
 
		finishWritingRaw(XTime::now(), XTime::now());
	}
	afterStop();	
	return NULL;
}
