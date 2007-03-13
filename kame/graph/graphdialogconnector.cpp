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
#include "graphdialogconnector.h"
#include "graphdialog.h"
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qpainter.h>
#include <qtooltip.h>
#include <qtabwidget.h>
#include <qimage.h>
#include <qlayout.h>
#include <klocale.h>
#include <knuminput.h>
#include <qcheckbox.h>

XQGraphDialogConnector::XQGraphDialogConnector
(const shared_ptr<XGraph> &graph, DlgGraphSetup* item) :
    XQConnector(graph, item),
    m_pItem(item),
    m_selPlot(createOrphan<XItemNode<XPlotList, XPlot> >("", true, graph->plots(), true)),
    m_selAxis(createOrphan<XItemNode<XAxisList, XAxis> >("", true, graph->axes(), true)),
    m_conBackGround(xqcon_create<XKColorComboConnector>
					(graph->backGround(), m_pItem->m_clrBackGroundColor)),
    m_conPersistence(xqcon_create<XKDoubleNumInputConnector>
					 (graph->persistence(), m_pItem->m_dblPersistence)),
    m_conPlots(xqcon_create<XQListBoxConnector>(m_selPlot, m_pItem->lbPlots)),
    m_conAxes(xqcon_create<XQListBoxConnector>(m_selAxis, m_pItem->lbAxes))
{
    m_pItem->dblIntensity->setRange(0.0, 2.0, 0.1, true);
    m_pItem->m_dblPersistence->setRange(0.0, 1.0, 0.1, true);
    
    m_lsnAxisChanged = m_selAxis->onValueChanged().connectWeak
        (shared_from_this(), &XQGraphDialogConnector::onSelAxisChanged, XListener::FLAG_MAIN_THREAD_CALL);
    m_lsnPlotChanged = m_selPlot->onValueChanged().connectWeak
        (shared_from_this(), &XQGraphDialogConnector::onSelPlotChanged, XListener::FLAG_MAIN_THREAD_CALL);

    m_pItem->show();
}   
XQGraphDialogConnector::~XQGraphDialogConnector() {
    if(isItemAlive()) m_pItem->close();
}
 
void
XQGraphDialogConnector::onSelAxisChanged(const shared_ptr<XValueNodeBase> &) {
    m_conAutoScale.reset();
    m_conLogScale.reset();
    m_conDisplayTicLabels.reset();
    m_conDisplayMajorTics.reset();
    m_conDisplayMinorTics.reset();
    m_conAxisMin.reset();
    m_conAxisMax.reset();
    m_conTicLabelFormat.reset();
	shared_ptr<XAxis> axis = *m_selAxis;
	if(!axis) {
		return;
	}
	m_conAutoScale = xqcon_create<XQToggleButtonConnector>(
		axis->autoScale(), m_pItem->ckbAutoScale);
	m_conLogScale = xqcon_create<XQToggleButtonConnector>(
		axis->logScale(), m_pItem->ckbLogScale);
	m_conDisplayTicLabels = xqcon_create<XQToggleButtonConnector>
		(axis->displayTicLabels(), m_pItem->ckbDisplayTicLabels);
	m_conDisplayMajorTics = xqcon_create<XQToggleButtonConnector>
		(axis->displayMajorTics(), m_pItem->ckbDisplayMajorTics);
	m_conDisplayMinorTics = xqcon_create<XQToggleButtonConnector>
		(axis->displayMinorTics(), m_pItem->ckbDisplayMinorTics);
	m_conAxisMin = xqcon_create<XQLineEditConnector>(axis->minValue(), m_pItem->edAxisMin);
	m_conAxisMax = xqcon_create<XQLineEditConnector>(axis->maxValue(), m_pItem->edAxisMax);
	m_conTicLabelFormat = xqcon_create<XQLineEditConnector>
		(axis->ticLabelFormat(), m_pItem->edTicLabelFormat);
}
void
XQGraphDialogConnector::onSelPlotChanged(const shared_ptr<XValueNodeBase> &) {
    m_conDrawPoints.reset();
    m_conDrawLines.reset();
    m_conDrawBars.reset();
    m_conDisplayMajorGrids.reset();
    m_conDisplayMinorGrids.reset();
    m_conMajorGridColor.reset();
    m_conMinorGridColor.reset();
    m_conPointColor.reset();
    m_conLineColor.reset();
    m_conBarColor.reset();
    m_conMaxCount.reset();
    m_conClearPoints.reset();
    m_conIntensity.reset();
    m_conColorPlot.reset();
    m_conColorPlotColorLow.reset();
    m_conColorPlotColorHigh.reset();

	shared_ptr<XPlot> plot = *m_selPlot;
	if(!plot) {
		return;
	}
	m_conDrawPoints = xqcon_create<XQToggleButtonConnector>
		(plot->drawPoints(), m_pItem->ckbDrawPoints);
	m_conDrawLines = xqcon_create<XQToggleButtonConnector>
		(plot->drawLines(), m_pItem->ckbDrawLines);
	m_conDrawBars = xqcon_create<XQToggleButtonConnector>
		(plot->drawBars(), m_pItem->ckbDrawBars);
	m_conDisplayMajorGrids = xqcon_create<XQToggleButtonConnector>
		(plot->displayMajorGrid(), m_pItem->ckbDisplayMajorGrids);
	m_conDisplayMinorGrids = xqcon_create<XQToggleButtonConnector>
		(plot->displayMinorGrid(), m_pItem->ckbDisplayMinorGrids);
	m_conMajorGridColor = xqcon_create<XKColorComboConnector>
		(plot->majorGridColor(), m_pItem->clrMajorGridColor);
	m_conMinorGridColor = xqcon_create<XKColorComboConnector>
		(plot->minorGridColor(), m_pItem->clrMinorGridColor);
	m_conPointColor = xqcon_create<XKColorComboConnector>
		(plot->pointColor(), m_pItem->clrPointColor);
	m_conLineColor = xqcon_create<XKColorComboConnector>
		(plot->lineColor(), m_pItem->clrLineColor);
	m_conBarColor = xqcon_create<XKColorComboConnector>
		(plot->barColor(), m_pItem->clrBarColor);
	m_conMaxCount = xqcon_create<XQLineEditConnector>
		(plot->maxCount(), m_pItem->edMaxCount);
	m_conClearPoints = xqcon_create<XQButtonConnector>
		(plot->clearPoints(), m_pItem->btnClearPoints);
	m_conIntensity = xqcon_create<XKDoubleNumInputConnector>
		(plot->intensity(), m_pItem->dblIntensity);
	m_conColorPlot = xqcon_create<XQToggleButtonConnector>
		(plot->colorPlot(), m_pItem->ckbColorPlot);
	m_conColorPlotColorLow = xqcon_create<XKColorComboConnector>
		(plot->colorPlotColorLow(), m_pItem->clrColorPlotLow);
	m_conColorPlotColorHigh = xqcon_create<XKColorComboConnector>
		(plot->colorPlotColorHigh(), m_pItem->clrColorPlotHigh);
}


