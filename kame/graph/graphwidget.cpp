#include "graphwidget.h"
#include "graph.h"
#include <qpixmap.h>
#include <qpainter.h>
#include <qimage.h>
#include "graphpainter.h"
#include <qlayout.h>
#include <klocale.h>
#include "measure.h"
#include "graphdialogconnector.h"
#include "forms/graphdialog.h"

XQGraph::XQGraph( QWidget* parent, const char* name, WFlags fl ) :
    QGLWidget( QGLFormat(AlphaChannel | DoubleBuffer | Rgba | DepthBuffer | AccumBuffer )
    	, parent, name, 0, fl)
{
    if(!format().directRendering()) dbgPrint("direct rendering disabled");
//      if(!layout() ) new QHBoxLayout(this);
//      layout()->setAutoAdd(true);
}
XQGraph::~XQGraph()
{
    m_painter.reset();
}
void
XQGraph::setGraph(const shared_ptr<XGraph> &graph)
{
    m_conDialog.reset();
    m_painter.reset();
    m_graph = graph;
    if(graph && isVisible() && isShown()) {
            showEvent(NULL);
    } 
}
void
XQGraph::mousePressEvent ( QMouseEvent* e)
{
  if(!m_painter ) return;
  XQGraphPainter::SelectionMode mode;
  switch (e->button()) {
  case RightButton:
	mode = XQGraphPainter::SelAxis;
	break;
  case LeftButton:
	mode = XQGraphPainter::SelPlane;
	break;
  case MidButton:
	mode = XQGraphPainter::TiltTracking;
	break;
  default:
	mode = XQGraphPainter::SelNone;
	break;
  }
  m_painter->selectObjs(e->pos().x(), e->pos().y(), XQGraphPainter::SelStart, mode);
}
void
XQGraph::mouseMoveEvent ( QMouseEvent* e)
{
  static XTime lasttime = XTime::now();
  if(XTime::now() - lasttime < 0.033) return;
  if(!m_painter ) return;
  m_painter->selectObjs(e->pos().x(), e->pos().y(), XQGraphPainter::Selecting);  
}
void
XQGraph::mouseReleaseEvent ( QMouseEvent* e)
{
  if(!m_painter ) return;
  m_painter->selectObjs(e->pos().x(), e->pos().y(), XQGraphPainter::SelFinish);
}
void
XQGraph::mouseDoubleClickEvent ( QMouseEvent* e)
{
  e->accept();
  if(!m_painter ) return;
  if(m_graph) { 
   switch (e->button()) {
     case RightButton:
      	m_painter->showHelp();
    	break;
     case LeftButton:
        m_conDialog = xqcon_create<XQGraphDialogConnector>(m_graph,
            new DlgGraphSetup(this, "GraphDialog", false, Qt::WDestructiveClose));
    	break;
    case MidButton:
    	break;
    default:
    	break;
      }
  }
}
void
XQGraph::wheelEvent ( QWheelEvent *e)
{
  e->accept();
  if(m_painter )
      m_painter->wheel(e->pos().x(), e->pos().y(), (double)e->delta() / 8.0);
}
void
XQGraph::showEvent ( QShowEvent * )
{
  shared_ptr<XGraph> graph = m_graph;
  if(graph) { 
      m_painter.reset();
      // m_painter will be re-set in the constructor.
      new XQGraphPainter(graph, this);
      glInit();
      setMouseTracking(true);
  }
}
void
XQGraph::hideEvent ( QHideEvent * )
{
  m_conDialog.reset();
  m_painter.reset();
  setMouseTracking(false);
}
  //! openGL stuff
void
XQGraph::initializeGL ()
{
    glClearColor( 1.0, 1.0, 1.0, 1.0 );
    glClearDepth( 1.0 );
    if(m_painter )
        m_painter->initializeGL();
}
void
XQGraph::resizeGL ( int width, int height )
{
    glMatrixMode(GL_PROJECTION);
    glViewport( 0, 0, (GLint)width, (GLint)height ); 
    if(m_painter )
        m_painter->resizeGL(width, height);
}
void
XQGraph::paintGL ()
{
    if(m_painter )
        m_painter->paintGL();
    glEnd();
}
