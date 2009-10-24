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
#include "entrylistconnector.h"
#include "analyzer.h"
#include "driver.h"
#include <q3table.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <knuminput.h>

//---------------------------------------------------------------------------

XEntryListConnector::XEntryListConnector
(const shared_ptr<XScalarEntryList> &node, Q3Table *item, const shared_ptr<XChartList> &chartlist)
	: XListQConnector(node, item),
	  m_chartList(chartlist)
{
	connect(item, SIGNAL( clicked( int, int, int, const QPoint& )),
			this, SLOT(clicked( int, int, int, const QPoint& )) );
	m_pItem->setNumCols(4);
	const double def = 50;
	m_pItem->setColumnWidth(0, (int)(def * 2.5));
	m_pItem->setColumnWidth(1, (int)(def * 2.0));
	m_pItem->setColumnWidth(2, (int)(def * 0.8));
	m_pItem->setColumnWidth(3, (int)(def * 2.5));
	QStringList labels;
	labels += i18n("Entry");
	labels += i18n("Value");
	labels += i18n("Store");
	labels += i18n("Delta");
	m_pItem->setColumnLabels(labels);
  
	XNode::NodeList::reader list(node->children());
	if(list) {
		for(XNode::NodeList::const_iterator it = list->begin(); it != list->end(); it++)
			onCatch(*it);
	}
}
void
XEntryListConnector::onRecord(const shared_ptr<XDriver> &driver)
{
	for(tconslist::iterator it = m_cons.begin(); it != m_cons.end(); it++)
	{
		if((*it)->entry->driver() == driver)
		{
			tcons::tlisttext text;
			text.label = (*it)->label;
			text.str.reset(new XString((*it)->entry->value()->to_str()));
			(*it)->tlkOnRecordRedirected->talk(text);
		}
	}
}
void
XEntryListConnector::tcons::onRecordRedirected(const tlisttext &text)
{
    text.label->setText(text.str->c_str());
}

void
XEntryListConnector::clicked ( int row, int col, int, const QPoint& ) {
	switch(col) {
	case 0:
	case 1:
	{
		XNode::NodeList::reader list(m_chartList->children());
		if(list) {
			if((row >= 0) && (row < (int)list->size())) {
				dynamic_pointer_cast<XValChart>(list->at(row))->showChart();
			}
		}
	}
	break;
	default:
		break;
	}
}
void
XEntryListConnector::onRelease(const shared_ptr<XNode> &node)
{
	for(tconslist::iterator it = m_cons.begin(); it != m_cons.end();)
	{
		ASSERT(m_pItem->numRows() == (int)m_cons.size());
		if((*it)->entry == node)
		{
			(*it)->driver->onRecord().disconnect(m_lsnOnRecord);
			for(int i = 0; i < m_pItem->numRows(); i++)
			{
				if(m_pItem->cellWidget(i, 1) == (*it)->label) m_pItem->removeRow(i);
			}
			it = m_cons.erase(it);
		}
		else
		{
			it++;
		}
	}
}
void
XEntryListConnector::onCatch(const shared_ptr<XNode> &node)
{
	shared_ptr<XScalarEntry> entry = dynamic_pointer_cast<XScalarEntry>(node);
	int i = m_pItem->numRows();
	m_pItem->insertRows(i);
	m_pItem->setText(i, 0, entry->getLabel().c_str());

	shared_ptr<XDriver> driver = entry->driver();
	if(m_lsnOnRecord)
		driver->onRecord().connect(m_lsnOnRecord);
	else
		m_lsnOnRecord = driver->onRecord().connectWeak(
			shared_from_this(), &XEntryListConnector::onRecord);

	m_cons.push_back(shared_ptr<tcons>(new tcons));
	m_cons.back()->entry = entry;
	m_cons.back()->label = new QLabel(m_pItem);
	m_pItem->setCellWidget(i, 1, m_cons.back()->label);
	QCheckBox *ckbStore = new QCheckBox(m_pItem);
	m_cons.back()->constore = xqcon_create<XQToggleButtonConnector>(entry->store(), ckbStore);
	m_pItem->setCellWidget(i, 2, ckbStore);
	QDoubleSpinBox *numDelta = new QDoubleSpinBox(m_pItem);
	numDelta->setRange(-1, 1e4);
	numDelta->setSingleStep(1);
	numDelta->setValue(0);
	numDelta->setDecimals(5);
	m_cons.back()->condelta = xqcon_create<XQDoubleSpinBoxConnector>(entry->delta(), numDelta);
	m_pItem->setCellWidget(i, 3, numDelta);
	m_cons.back()->driver = driver;
	m_cons.back()->tlkOnRecordRedirected.reset(new XTalker<tcons::tlisttext>);
	m_cons.back()->lsnOnRecordRedirected = m_cons.back()->tlkOnRecordRedirected->connectWeak(
		m_cons.back(), &XEntryListConnector::tcons::onRecordRedirected,
		XListener::FLAG_MAIN_THREAD_CALL | XListener::FLAG_AVOID_DUP | XListener::FLAG_DELAY_ADAPTIVE);
  

	ASSERT(m_pItem->numRows() == (int)m_cons.size());
}

