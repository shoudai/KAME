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
#include "graphwidget.h"
#include "graph.h"
#include <QDialog>
#include <QPixmap>
#include <QPainter>
#include <QImage>
#include <QLayout>
#include <QMouseEvent>
#include "graphpainter.h"
#include "measure.h"
#include "graphdialogconnector.h"
#include "ui_graphdialog.h"

typedef QForm<QDialog, Ui_DlgGraphSetup> DlgGraphSetup;

XQGraph::XQGraph( QWidget* parent, Qt::WindowFlags fl ) :
    QOpenGLWidget(parent) {
//    if( !parent->layout() ) {
//        parent->setLayout(new QHBoxLayout(this));
//        parent->layout()->addWidget(this);
//    }
    setMouseTracking(true);

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(16);
    format.setSamples(2);
    format.setVersion(2, 0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
//    format.setProfile(QSurfaceFormat::CoreProfile);

//    QGLFormat(QGL::AlphaChannel | QGL::DoubleBuffer | QGL::Rgba |
//                             QGL::DepthBuffer | QGL::AccumBuffer |
//                             QGL::SampleBuffers
    setFormat(format);
}
XQGraph::~XQGraph() {
    m_painter.reset();
}
void
XQGraph::setGraph(const shared_ptr<XGraph> &graph) {
    m_conDialog.reset();
    m_painter.reset();
    m_graph = graph;
//    if(graph && !isHidden()) {
//		showEvent(NULL);
//    }
}
void
XQGraph::mousePressEvent ( QMouseEvent* e) {
	if( !m_painter ) return;
	XQGraphPainter::SelectionMode mode;
	switch (e->button()) {
	case Qt::RightButton:
        mode = XQGraphPainter::SelectionMode::SelAxis;
		break;
	case Qt::LeftButton:
        mode = XQGraphPainter::SelectionMode::SelPlane;
		break;
	case Qt::MidButton:
        mode = XQGraphPainter::SelectionMode::TiltTracking;
		break;
	default:
        mode = XQGraphPainter::SelectionMode::SelNone;
		break;
	}
    m_painter->selectObjs(e->pos().x(), e->pos().y(), XQGraphPainter::SelectionState::SelStart, mode);
}
void
XQGraph::mouseMoveEvent ( QMouseEvent* e) {
	static XTime lasttime = XTime::now();
	if(XTime::now() - lasttime < 0.033) return;
	if( !m_painter ) return;
    m_painter->selectObjs(e->pos().x(), e->pos().y(), XQGraphPainter::SelectionState::Selecting);
}
void
XQGraph::mouseReleaseEvent ( QMouseEvent* e) {
	if( !m_painter ) return;
    m_painter->selectObjs(e->pos().x(), e->pos().y(), XQGraphPainter::SelectionState::SelFinish);
}
void
XQGraph::mouseDoubleClickEvent ( QMouseEvent* e) {
	e->accept();
	if( !m_painter ) return;
	if(m_graph) { 
		switch (e->button()) {
		case Qt::RightButton:
			m_painter->showHelp();
			break;
		case Qt::LeftButton:
			m_conDialog = xqcon_create<XQGraphDialogConnector>(
				m_graph,
                new DlgGraphSetup(parentWidget()));
            //\todo setAttribute Qt::WA_DeleteOnClose
			break;
		case Qt::MidButton:
			break;
		default:
			break;
		}
	}
}
void
XQGraph::wheelEvent ( QWheelEvent *e) {
	e->accept();
	if(m_painter )
		m_painter->wheel(e->pos().x(), e->pos().y(), (double)e->delta() / 8.0);
}
void
XQGraph::showEvent ( QShowEvent *) {
    shared_ptr<XGraph> graph = m_graph;
	if(graph) { 
		m_painter.reset();
		// m_painter will be re-set in the constructor.
		new XQGraphPainter(graph, this);
//        setMouseTracking(true);
        initializeGL();
    }
}
void
XQGraph::hideEvent ( QHideEvent * ) {
	m_conDialog.reset();
	m_painter.reset();
//    setMouseTracking(false);
}
//! openGL stuff
void
XQGraph::initializeGL() {
    if(m_painter )
        m_painter->initializeGL();
}
void
XQGraph::resizeGL ( int width, int height ) {
    // be aware of retina display.
    double pixel_ratio = devicePixelRatio();
    glViewport( 0, 0, (GLint)(width * pixel_ratio),
                (GLint)(height * pixel_ratio));
    if(m_painter )
        m_painter->resizeGL(width, height);
}

void
XQGraph::paintGL() {
    if(m_painter )
        m_painter->paintGL();
}
