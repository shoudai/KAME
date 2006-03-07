#include "measure.h"#include "kame.h"#include "xrubysupport.h"#include "driver.h"#include "interface.h"#include "analyzer.h"#include "recorder.h"#include "recordreader.h"#include "thermometer/thermometer.h"#include "thermometer/caltable.h"#include "driver/driverlistconnector.h"#include "driver/interfacelistconnector.h"#include "analyzer/analyzer.h"#include "analyzer/entrylistconnector.h"#include "analyzer/graphlistconnector.h"#include "analyzer/recordreaderconnector.h"#include "graphtool.h"#include "interfacetool.h"#include "drivertool.h"#include "scalarentrytool.h"#include <qstring.h>#include <qtextbrowser.h>#include <qpushbutton.h>#include <qcheckbox.h>#include <kfiledialog.h>#include <kstandarddirs.h>#include <kmessagebox.h>#define THERMO_FILE "thermometers.xml"shared_ptr<XStatusPrinter> g_statusPrinter;XMeasure::XMeasure(const char *name, bool runtime) :    XNode(name, runtime),   m_thermometers(create<XThermometerList>("Thermometers", false)),   m_scalarEntries(create<XScalarEntryList>("ScalarEntries", true)),   m_graphList(create<XGraphList>("GraphList", true, scalarEntryList())),   m_chartList(create<XChartList>("ChartList", true, scalarEntryList())),   m_interfaces(create<XInterfaceList>("Interfaces", true)),   m_drivers(create<XDriverList>("Drivers", false,         scalarEntryList(), interfaceList(), thermometerList())),   m_textWriter(create<XTextWriter>("TextWriter", false, driverList(), scalarEntryList())),   m_rawStreamRecorder(create<XRawStreamRecorder>("RawStreamRecorder", false, driverList())),   m_rawStreamRecordReader(create<XRawStreamRecordReader>("RawStreamRecordReader", false,         driverList())),   m_conRecordReader(xqcon_create<XRawStreamRecordReaderConnector>(            rawStreamRecordReader(),             dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmRecordReader)),   m_conDrivers(xqcon_create<XDriverListConnector>(            m_drivers, dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmDriver)),   m_conInterfaces(xqcon_create<XInterfaceListConnector>(            m_interfaces,            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmInterface->tblInterfaces)),   m_conEntries(xqcon_create<XEntryListConnector>(            scalarEntryList(),             dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmScalarEntry->m_tblEntries,            chartList())),   m_conGraphs(xqcon_create<XGraphListConnector>(graphList(),            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmGraphList->tblGraphs,            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmGraphList->btnNewGraph,            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmGraphList->btnDeleteGraph)),   m_conTextWrite(xqcon_create<XQToggleButtonConnector>(            textWriter()->recording(),            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmScalarEntry->m_ckbTextWrite)),   m_conTextURL(xqcon_create<XKURLReqConnector>(            textWriter()->filename(),            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmScalarEntry->m_urlTextWriter,            "*.dat|Data files (*.dat)\n*.*|All files (*.*)", true)),   m_conTextLastLine(xqcon_create<XQLineEditConnector>(            textWriter()->lastLine(),            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmScalarEntry->m_edLastLine)),   m_conBinURL(xqcon_create<XKURLReqConnector>(            rawStreamRecorder()->filename(),            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmDriver->m_urlBinRec,            "*.bin|Binary files (*.bin)\n*.*|All files (*.*)", true)),   m_conBinWrite(xqcon_create<XQToggleButtonConnector>(            rawStreamRecorder()->recording(),            dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmDriver->m_ckbBinRecWrite)),   m_conUrlRubyThread(),   m_conCalTable(xqcon_create<XConCalTable>(            m_thermometers, dynamic_cast<FrmKameMain*>(g_pFrmMain)->m_pFrmCalTable)){  g_statusPrinter = XStatusPrinter::create();     m_lsnOnReleaseDriver = driverList()->onRelease().connectWeak(        false, shared_from_this(), &XMeasure::onReleaseDriver);      m_ruby = createOrphan<XRuby>("RubySupport", true,    dynamic_pointer_cast<XMeasure>(shared_from_this()));    m_ruby->resume();  initialize();}XMeasure::~XMeasure(){     printf("terminate\n");      m_rawStreamRecordReader->terminate();  m_ruby->terminate();  m_ruby.reset();  g_statusPrinter.reset();}voidXMeasure::initialize(){    if(!thermometerList()->getChild("Raw"))        thermometerList()->create<XRawThermometer>("Raw", true);}voidXMeasure::terminate(){    interfaceList()->clearChildren();    driverList()->clearChildren();    thermometerList()->clearChildren();    initialize();}voidXMeasure::stop(){  atomic_shared_ptr<const XNode::NodeList> list(driverList()->children());  if(list) {       for(XNode::NodeList::const_iterator it = list->begin(); it != list->end(); it++) {          shared_ptr<XDriver> driver = dynamic_pointer_cast<XDriver>(*it);            driver->stopMeas();      }  }}voidXMeasure::onReleaseDriver(const shared_ptr<XNode> &d){    shared_ptr<XDriver> driver = dynamic_pointer_cast<XDriver>(d);    ASSERT( driver );    driver->stopMeas();    for(;;) {        shared_ptr<XScalarEntry> entry;        atomic_shared_ptr<const XNode::NodeList> list(scalarEntryList()->children());        if(list) {             for(XNode::NodeList::const_iterator it = list->begin(); it != list->end(); it++) {              shared_ptr<XScalarEntry> _entry = dynamic_pointer_cast<XScalarEntry>(*it);              if(_entry->driver() == driver) {                    entry = _entry;              }            }        }        if(!entry) break;        scalarEntryList()->releaseChild(entry);    }    for(;;) {        shared_ptr<XInterface> interface;        atomic_shared_ptr<const XNode::NodeList> list(interfaceList()->children());        if(list) {             for(XNode::NodeList::const_iterator it = list->begin(); it != list->end(); it++) {              shared_ptr<XInterface> _interface = dynamic_pointer_cast<XInterface>(*it);              if(_interface->driver() == driver) {                        interface = _interface;              }            }        }        if(!interface) break;        interfaceList()->releaseChild(interface);    }}