/***************************************************************************
		Copyright (C) 2002-2007 Kentaro Kitagawa
		                   kitagawa@scphys.kyoto-u.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
 ***************************************************************************/
#include "dcsourceform.h"
#include "dcsource.h"
#include "xnodeconnector.h"
#include <qstatusbar.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <klocale.h>

XDCSource::XDCSource(const char *name, bool runtime, 
   const shared_ptr<XScalarEntryList> &scalarentries,
   const shared_ptr<XInterfaceList> &interfaces,
   const shared_ptr<XThermometerList> &thermometers,
   const shared_ptr<XDriverList> &drivers) : 
    XPrimaryDriver(name, runtime, scalarentries, interfaces, thermometers, drivers),
    m_function(create<XComboNode>("Function", false)),
    m_output(create<XBoolNode>("Output", true)),
    m_value(create<XDoubleNode>("Value", false)),
    m_channel(create<XComboNode>("Channel", false, true)),
    m_form(new FrmDCSource(g_pFrmMain))
{
  m_form->statusBar()->hide();
  m_form->setCaption(KAME::i18n("DC Source - ") + getLabel() );

  m_output->setUIEnabled(false);
  m_function->setUIEnabled(false);
  m_value->setUIEnabled(false);
  m_channel->setUIEnabled(false);

  m_conFunction = xqcon_create<XQComboBoxConnector>(m_function, m_form->m_cmbFunction);
  m_conOutput = xqcon_create<XQToggleButtonConnector>(m_output, m_form->m_ckbOutput);
  m_conValue = xqcon_create<XQLineEditConnector>(m_value, m_form->m_edValue);
  m_conChannel = xqcon_create<XQComboBoxConnector>(m_channel, m_form->m_cmbChannel);
}

void
XDCSource::showForms() {
//! impliment form->show() here
    m_form->show();
    m_form->raise();
}

void
XDCSource::start()
{
  m_output->setUIEnabled(true);
  m_function->setUIEnabled(true);
  m_value->setUIEnabled(true);
  m_channel->setUIEnabled(true);
        
  m_lsnOutput = output()->onValueChanged().connectWeak(
                          shared_from_this(), &XDCSource::onOutputChanged);
  m_lsnFunction = function()->onValueChanged().connectWeak(
                          shared_from_this(), &XDCSource::onFunctionChanged);
  m_lsnValue = value()->onValueChanged().connectWeak(
                        shared_from_this(), &XDCSource::onValueChanged);
  m_lsnChannel = channel()->onValueChanged().connectWeak(
                          shared_from_this(), &XDCSource::onChannelChanged);
}
void
XDCSource::stop()
{
  m_lsnChannel.reset();
  m_lsnOutput.reset();
  m_lsnFunction.reset();
  m_lsnValue.reset();
  
  m_output->setUIEnabled(false);
  m_function->setUIEnabled(false);
  m_value->setUIEnabled(false);
  m_channel->setUIEnabled(false);
  
  afterStop();
}

void
XDCSource::analyzeRaw() throw (XRecordError&)
{
}
void
XDCSource::visualize()
{
 //! impliment extra codes which do not need write-lock of record
 //! record is read-locked
}

void 
XDCSource::onOutputChanged(const shared_ptr<XValueNodeBase> &)
{
	int ch = *channel();
    try {
        changeOutput(ch, *output());
    }
    catch (XKameError& e) {
        e.print(getLabel() + KAME::i18n(": Error while changing output, "));
        return;
    }
}
void 
XDCSource::onFunctionChanged(const shared_ptr<XValueNodeBase> &)
{
	int ch = *channel();
    try {
        changeFunction(ch, *function());
    }
    catch (XKameError& e) {
        e.print(getLabel() + KAME::i18n(": Error while changing function, "));
        return;
    }
}
void 
XDCSource::onValueChanged(const shared_ptr<XValueNodeBase> &)
{
	int ch = *channel();
    try {
        changeValue(ch, *value());
    }
    catch (XKameError& e) {
        e.print(getLabel() + KAME::i18n(": Error while changing value, "));
        return;
    }
}
void 
XDCSource::onChannelChanged(const shared_ptr<XValueNodeBase> &)
{
	int ch = *channel();
    try {
    	output()->onValueChanged().mask();
    	function()->onValueChanged().mask();
    	value()->onValueChanged().mask();
        queryStatus(ch);
    	output()->onValueChanged().unmask();
    	function()->onValueChanged().unmask();
    	value()->onValueChanged().unmask();
    }
    catch (XKameError& e) {
        e.print(getLabel() + KAME::i18n(": Error while changing value, "));
        return;
    }
}
