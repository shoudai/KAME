/***************************************************************************
		Copyright (C) 2002-2008 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp
		
		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.
		
		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef pulserdriverconnectorH
#define pulserdriverconnectorH

#include "xnodeconnector.h"
#include "pulserdriver.h"
//---------------------------------------------------------------------------

class QTable;
class XQGraph;
class XXYPlot;
class XGraph;

class XQPulserDriverConnector : public XQConnector
{
	Q_OBJECT
	XQCON_OBJECT
protected:
	XQPulserDriverConnector(const shared_ptr<XPulser> &node, Q3Table *item, XQGraph *graph);
public:
	virtual ~XQPulserDriverConnector();

protected slots:
void clicked ( int row, int col, int button, const QPoint & mousePos );
	void selectionChanged ();
private:

	void updateGraph(bool checkselection);
  
	shared_ptr<XListener> m_lsnOnPulseChanged;
	void onPulseChanged(const shared_ptr<XDriver> &);
  
	Q3Table *const m_pTable;
	const weak_ptr<XPulser> m_pulser;

	const shared_ptr<XGraph> m_graph;
	shared_ptr<XXYPlot> m_barPlot;
	std::deque<shared_ptr<XXYPlot> > m_plots;
};

#endif