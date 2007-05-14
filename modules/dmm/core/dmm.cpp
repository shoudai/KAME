/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
//---------------------------------------------------------------------------
#include "forms/dmmform.h"
#include "dmm.h"
#include "interface.h"
#include "analyzer.h"
#include "xnodeconnector.h"
#include <qstatusbar.h>
#include <klocale.h>

XDMM::XDMM(const char *name, bool runtime, 
		   const shared_ptr<XScalarEntryList> &scalarentries,
		   const shared_ptr<XInterfaceList> &interfaces,
		   const shared_ptr<XThermometerList> &thermometers,
		   const shared_ptr<XDriverList> &drivers) : 
    XPrimaryDriver(name, runtime, scalarentries, interfaces, thermometers, drivers),
    m_entry(create<XScalarEntry>("Value", false, 
								 dynamic_pointer_cast<XDriver>(shared_from_this()))),
    m_function(create<XComboNode>("Function", false)),
    m_waitInms(create<XUIntNode>("WaitInms", false)),
    m_form(new FrmDMM(g_pFrmMain))
{
	scalarentries->insert(m_entry);
	m_waitInms->value(100);
	m_form->statusBar()->hide();
	m_form->setCaption(KAME::i18n("DMM - ") + getLabel() );
	m_function->setUIEnabled(false);
	m_waitInms->setUIEnabled(false);
	m_conFunction = xqcon_create<XQComboBoxConnector>(m_function, m_form->m_cmbFunction);
	m_conWaitInms = xqcon_create<XQSpinBoxConnector>(m_waitInms, m_form->m_numWait);
}

void
XDMM::showForms() {
//! impliment form->show() here
    m_form->show();
    m_form->raise();
}

void
XDMM::start()
{
    m_thread.reset(new XThread<XDMM>(shared_from_this(), &XDMM::execute));
    m_thread->resume();

    m_function->setUIEnabled(true);
    m_waitInms->setUIEnabled(true);
}
void
XDMM::stop()
{
    m_function->setUIEnabled(false);
    m_waitInms->setUIEnabled(false);

    if(m_thread) m_thread->terminate();
//    m_thread->waitFor();
//  thread must do interface()->close() at the end
}

void
XDMM::analyzeRaw() throw (XRecordError&)
{
    double x;
    x = pop<double>();
    m_value = x;
    m_entry->value(x);
}
void
XDMM::visualize()
{
	//! impliment extra codes which do not need write-lock of record
	//! record is read-locked
}

//! called when m_function is changed
void
XDMM::onFunctionChanged(const shared_ptr<XValueNodeBase> &node)
{
    try {
        changeFunction();
    }
    catch (XKameError &e) {
		e.print(getLabel() + " " + KAME::i18n("DMM Error"));
    }
}

void *
XDMM::execute(const atomic<bool> &terminated)
{
    try {
        changeFunction();
    }
    catch (XKameError &e) {
		e.print(getLabel() + " " + KAME::i18n("DMM Error"));
		afterStop();
		return NULL;
    }
        
    m_lsnOnFunctionChanged = 
        function()->onValueChanged().connectWeak(
			shared_from_this(), &XDMM::onFunctionChanged);    
	while(!terminated)
	{
		msecsleep(*waitInms());
		if(function()->to_str().empty()) continue;
      
		double x;
		XTime time_awared = XTime::now();
		// try/catch exception of communication errors
		try {
			x = oneShotRead();
		}
		catch (XKameError &e) {
			e.print(getLabel() + " " + KAME::i18n("DMM Read Error"));
			continue;
		}
		clearRaw();
		push(x);
		finishWritingRaw(time_awared, XTime::now());
	}
    
    m_lsnOnFunctionChanged.reset();
        
    afterStop();
	return NULL;
}

