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
//---------------------------------------------------------------------------
#include "ui_graphform.h"
#include "graphwidget.h"
#include "analyzer.h"
#include "graph.h"
#include "driver.h"
#include "measure.h"

#include <QStatusBar>

//---------------------------------------------------------------------------
XScalarEntry::XScalarEntry(const char *name, bool runtime, const shared_ptr<XDriver> &driver,
						   const char *format)
	: XNode(name, runtime),
	  m_driver(driver),
	  m_delta(create<XDoubleNode>("Delta", false)),
	  m_store(create<XBoolNode>("Store", false)),
	  m_value(create<XDoubleNode>("Value", true)),
	  m_storedValue(create<XDoubleNode>("StoredValue", true)),
	  m_bTriggered(false) {

	m_delta->setFormat(format);
	m_storedValue->setFormat(format);
	m_value->setFormat(format);
	store()->value(false);
}
bool
XScalarEntry::isTriggered() const {
    return m_bTriggered;
}
void
XScalarEntry::storeValue() {
    storedValue()->value(*value());
    m_bTriggered = false;
}

XString
XScalarEntry::getLabel() const {
	return driver()->getLabel() + "-" + XNode::getLabel();
}

void
XScalarEntry::value(double val) {
	if((*delta() != 0) && (fabs(val - *storedValue()) > *delta()))
	{
		m_bTriggered = true;
	}
	value()->value(val);
}

XValChart::XValChart(const char *name, bool runtime,
	const shared_ptr<XScalarEntry> &entry)
	: XNode(name, runtime),
	  m_entry(entry),
	  m_graph(create<XGraph>(name, false)),
	  m_graphForm(new FrmGraph(g_pFrmMain)) {

	m_graphForm->m_graphwidget->setGraph(m_graph);
    
	for(Transaction tr( *m_graph);; ++tr) {
		m_chart= m_graph->plots()->create<XXYPlot>(tr, entry->getName().c_str(), true, ref(tr), m_graph);
		tr[ *m_graph->persistence()] = 0.0;
		tr[ *m_chart->label()] = entry->getLabel();
		const XNode::NodeList &axes_list( *tr.list(m_graph->axes()));
		shared_ptr<XAxis> axisx = static_pointer_cast<XAxis>(axes_list.at(0));
		shared_ptr<XAxis> axisy = static_pointer_cast<XAxis>(axes_list.at(1));

		tr[ *m_chart->axisX()] = axisx;
		tr[ *m_chart->axisY()] = axisy;
		tr[ *m_chart->maxCount()] = 300;
		tr[ *axisx->length()] = 0.95 - tr[ *axisx->x()];
		tr[ *axisy->length()] = 0.90 - tr[ *axisy->y()];
		tr[ *axisx->label()] = "Time";
		tr[ *axisx->ticLabelFormat()] = "TIME:%T";
		tr[ *axisy->label()] = entry->getLabel();
		tr[ *axisy->ticLabelFormat()] = entry->value()->format();
		tr[ *axisx->minValue()].setUIEnabled(false);
		tr[ *axisx->maxValue()].setUIEnabled(false);
		tr[ *axisx->autoScale()].setUIEnabled(false);
		tr[ *axisx->logScale()].setUIEnabled(false);
		tr[ *m_graph->label()] = entry->getLabel();
		if(tr.commit())
			break;
	}

    m_lsnOnRecord = entry->driver()->onRecord().connectWeak(
        shared_from_this(), &XValChart::onRecord);
}
void
XValChart::onRecord(const shared_ptr<XDriver> &driver) {
	double val;
    val = *m_entry->value();
    XTime time = driver->time();
    if(time)
        m_chart->addPoint(time.sec() + time.usec() * 1e-6, val);
}
void
XValChart::showChart(void) {
	m_graphForm->setWindowTitle(i18n("Chart - ") + getLabel() );
	m_graphForm->show();
}

XChartList::XChartList(const char *name, bool runtime, const shared_ptr<XScalarEntryList> &entries)
	: XAliasListNode<XValChart>(name, runtime),
	  m_entries(entries) {
	for(Transaction tr( *entries);; ++tr) {
	    m_lsnOnCatchEntry = tr[ *entries].onCatch().connectWeakly(shared_from_this(), &XChartList::onCatchEntry);
	    m_lsnOnReleaseEntry = tr[ *entries].onRelease().connectWeakly(shared_from_this(), &XChartList::onReleaseEntry);
		if(tr.commit())
			break;
	}
}

void
XChartList::onCatchEntry(const Snapshot &shot, const XListNodeBase::Payload::CatchEvent &e) {
    shared_ptr<XScalarEntry> entry = static_pointer_cast<XScalarEntry>(e.caught);
    create<XValChart>(entry->getName().c_str(), true, entry);
}
void
XChartList::onReleaseEntry(const Snapshot &shot, const XListNodeBase::Payload::ReleaseEvent &e) {
	shared_ptr<XScalarEntry> entry = dynamic_pointer_cast<XScalarEntry>(e.released);
	for(Transaction tr( *this);; ++tr) {
		shared_ptr<XValChart> valchart;
		if(tr.size()) {
			const XNode::NodeList &list( *tr.list());
			for(XNode::const_iterator it = list.begin(); it != list.end(); it++) {
				shared_ptr<XValChart> chart = dynamic_pointer_cast<XValChart>( *it);
				if(chart->entry() == entry) valchart = chart;
			}
		}
		if( !valchart)
			break;
		if( !release(tr, valchart))
			continue;
		if(tr.commit())
			break;
	}
}

XValGraph::XValGraph(const char *name, bool runtime,
					 Transaction &tr_entries, const shared_ptr<XScalarEntryList> &entries)
	: XNode(name, runtime),
	  m_graph(),
	  m_graphForm(),
	  m_axisX(create<tAxis>("AxisX", false, ref(tr_entries), entries)),
	  m_axisY1(create<tAxis>("AxisY1", false, ref(tr_entries), entries)),
	  m_axisZ(create<tAxis>("AxisZ", false, ref(tr_entries), entries)) {
    m_lsnAxisChanged = axisX()->onValueChanged().connectWeak(
        shared_from_this(), &XValGraph::onAxisChanged,
		XListener::FLAG_MAIN_THREAD_CALL | XListener::FLAG_AVOID_DUP);
    axisY1()->onValueChanged().connect(m_lsnAxisChanged);
    axisZ()->onValueChanged().connect(m_lsnAxisChanged);
}
void
XValGraph::onAxisChanged(const shared_ptr<XValueNodeBase> &) {
    shared_ptr<XScalarEntry> entryx = *axisX();
    shared_ptr<XScalarEntry> entryy1 = *axisY1();
    shared_ptr<XScalarEntry> entryz = *axisZ();
    
	if(m_graph) releaseChild(m_graph);
	m_graph = create<XGraph>(getName().c_str(), false);
	m_graphForm.reset(new FrmGraph(g_pFrmMain));
	m_graphForm->m_graphwidget->setGraph(m_graph);

	if(!entryx || !entryy1) return;
  
	for(Transaction tr( *m_graph);; ++tr) {
		m_livePlot =
			m_graph->plots()->create<XXYPlot>(tr, (m_graph->getName() + "-Live").c_str(), false, ref(tr), m_graph);
		tr[ *m_livePlot->label()] = m_graph->getLabel() + " Live";
		m_storePlot =
			m_graph->plots()->create<XXYPlot>(tr, (m_graph->getName() + "-Stored").c_str(), false, ref(tr), m_graph);
		tr[ *m_storePlot->label()] = m_graph->getLabel() + " Stored";

		const XNode::NodeList &axes_list( *tr.list(m_graph->axes()));
		shared_ptr<XAxis> axisx = dynamic_pointer_cast<XAxis>(axes_list.at(0));
		shared_ptr<XAxis> axisy = dynamic_pointer_cast<XAxis>(axes_list.at(1));

		tr[ *axisx->ticLabelFormat()] = entryx->value()->format();
		tr[ *axisy->ticLabelFormat()] = entryy1->value()->format();
		tr[ *m_livePlot->axisX()] = axisx;
		tr[ *m_livePlot->axisY()] = axisy;
		tr[ *m_storePlot->axisX()] = axisx;
		tr[ *m_storePlot->axisY()] = axisy;

		tr[ *axisx->length()] = 0.95 - tr[ *axisx->x()];
		tr[ *axisy->length()] = 0.90 - tr[ *axisy->y()];
		if(entryz) {
			shared_ptr<XAxis> axisz = m_graph->axes()->create<XAxis>(
				tr, "Z Axis", false, XAxis::DirAxisZ, false, ref(tr), m_graph);
			tr[ *axisz->ticLabelFormat()] = entryz->value()->format();
			tr[ *m_livePlot->axisZ()] = axisz;
			tr[ *m_storePlot->axisZ()] = axisz;
	//	axisz->label()] = "Z Axis";
			tr[ *axisz->label()] = entryz->getLabel();
		}

		tr[ *m_storePlot->pointColor()] = clGreen;
		tr[ *m_storePlot->lineColor()] = clGreen;
		tr[ *m_storePlot->barColor()] = clGreen;
		tr[ *m_storePlot->displayMajorGrid()] = false;
		tr[ *m_livePlot->maxCount()] = 4000;
		tr[ *m_storePlot->maxCount()] = 4000;
		tr[ *axisx->label()] = entryx->getLabel();
		tr[ *axisy->label()] = entryy1->getLabel();
		tr[ *m_graph->label()] = getLabel();
		if(tr.commit())
			break;
	}


	m_lsnLiveChanged = entryx->value()->onValueChanged().connectWeak(
		shared_from_this(), &XValGraph::onLiveChanged);
	entryy1->value()->onValueChanged().connect(m_lsnLiveChanged);
	if(entryz) entryz->value()->onValueChanged().connect(m_lsnLiveChanged);
  
	m_lsnStoreChanged = entryx->storedValue()->onValueChanged().connectWeak(
		shared_from_this(), &XValGraph::onStoreChanged);
	entryy1->storedValue()->onValueChanged().connect(m_lsnStoreChanged);
	if(entryz) entryz->storedValue()->onValueChanged().connect(m_lsnStoreChanged);
  
	showGraph();
}

void
XValGraph::clearAllPoints() {
	if(!m_graph) return;
	m_storePlot->clearAllPoints();
	m_livePlot->clearAllPoints();
}
void
XValGraph::onLiveChanged(const shared_ptr<XValueNodeBase> &) {
	double x, y, z = 0.0;
    shared_ptr<XScalarEntry> entryx = *axisX();
    shared_ptr<XScalarEntry> entryy1 = *axisY1();
    shared_ptr<XScalarEntry> entryz = *axisZ();
    
    if(!entryx || !entryy1) return;
    x = *entryx->value();
    y = *entryy1->value();
    if(entryz) z = *entryz->value();
  
    m_livePlot->addPoint(x, y, z);
}
void
XValGraph::onStoreChanged(const shared_ptr<XValueNodeBase> &) {
	double x, y, z = 0.0;
    shared_ptr<XScalarEntry> entryx = *axisX();
    shared_ptr<XScalarEntry> entryy1 = *axisY1();
    shared_ptr<XScalarEntry> entryz = *axisZ();
    
    if(!entryx || !entryy1) return;
    x = *entryx->storedValue();
    y = *entryy1->storedValue();
    if(entryz) z = *entryz->storedValue();
  
    m_storePlot->addPoint(x, y, z);
}
void
XValGraph::showGraph() {
	if(m_graphForm) {
		m_graphForm->setWindowTitle(i18n("Graph - ") + getLabel() );
		m_graphForm->show();
	}
}

XGraphList::XGraphList(const char *name, bool runtime, const shared_ptr<XScalarEntryList> &entries)
	: XCustomTypeListNode<XValGraph>(name, runtime),
	  m_entries(entries) {
}

shared_ptr<XNode>
XGraphList::createByTypename(const XString &, const XString& name)  {
	shared_ptr<XValGraph> x;
	for(Transaction tr( *m_entries);; ++tr) {
		x = create<XValGraph>(name.c_str(), false, ref(tr), m_entries);
		if(tr.commit())
			break;
	}
	return x;
}
