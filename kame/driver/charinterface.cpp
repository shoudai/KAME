#include "charinterface.h"
//---------------------------------------------------------------------------
#include "measure.h"
#include "xnodeconnector.h"
#include <string>
#include <stdarg.h>
#include "driver.h"
#include "gpib.h"
#include "serial.h"
#include "dummyport.h"
#include "klocale.h"

//---------------------------------------------------------------------------
#define SNPRINT_BUF_SIZE 128

XThreadLocal<std::vector<char> > XPort::s_tlBuffer;


XCharInterface::XCharInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver) : 
    XInterface(name, runtime, driver), 
    m_bGPIBUseSerialPollOnWrite(true),
    m_bGPIBUseSerialPollOnRead(true),
    m_gpibWaitBeforeWrite(1),
    m_gpibWaitBeforeRead(2),
    m_gpibWaitBeforeSPoll(1),
    m_gpibMAVbit(0x10),
    m_script_send(create<XStringNode>("Send", true)),
    m_script_query(create<XStringNode>("Query", true))
{
#ifdef USE_GPIB
  device()->add("GPIB");
#endif
  device()->add("SERIAL");
  device()->add("DUMMY");
  baudrate()->value(9600);
  
  m_lsnOnSendRequested = m_script_send->onValueChanged().connectWeak(
            false, shared_from_this(), &XCharInterface::onSendRequested);
  m_lsnOnQueryRequested = m_script_query->onValueChanged().connectWeak(
            false, shared_from_this(), &XCharInterface::onQueryRequested);
}
void
XCharInterface::setEOS(const char *str) {
    m_eos = str;
}
         
void
XCharInterface::open() throw (XInterfaceError &)
{
  XScopedLock<XCharInterface> lock(*this);
  try {
      
      if(isOpened()) {
          throw XInterfaceError(KAME::i18n("Port has already opened"), __FILE__, __LINE__);
      }
        
      g_statusPrinter->printMessage(driver()->getLabel() + KAME::i18n(": Starting..."));
    
      {
      shared_ptr<XPort> port;
        #ifdef USE_GPIB
          if(device()->to_str() == "GPIB") {
            port.reset(new XGPIBPort(this));
          }
        #endif
          if(device()->to_str() == "SERIAL") {
            port.reset(new XSerialPort(this));
          }
          if(device()->to_str() == "DUMMY") {
            port.reset(new XDummyPort(this));
          }
          
          if(!port) throw XOpenInterfaceError(__FILE__, __LINE__);
            
          port->open();
          m_xport.swap(port);
      }
  }
  catch (XInterfaceError &e) {
          gErrPrint(driver()->getLabel() + KAME::i18n(": Opening port failed, because"));
          m_xport.reset();
          throw e;
  }
  //g_statusPrinter->clear();
}
void
XCharInterface::close()
{
  XScopedLock<XCharInterface> lock(*this);
//  if(isOpened()) 
//    g_statusPrinter->printMessage(QString(driver()->getLabel()) + KAME::i18n(": Stopping..."));
  m_xport.reset();
  //g_statusPrinter->clear();
}
int
XCharInterface::scanf(const char *fmt, ...) const {
  int ret;
  va_list ap;

  va_start(ap, fmt);

  ret = vsscanf(&buffer()[0], fmt, ap);

  va_end(ap);
  return ret;    
}
double
XCharInterface::toDouble() const throw (XCharInterface::XConvError &) {
    double x;
    int ret = sscanf(&buffer()[0], "%lf", &x);
    if(ret != 1) throw XConvError(__FILE__, __LINE__);
    return x;
}
int
XCharInterface::toInt() const throw (XCharInterface::XConvError &) {
    int x;
    int ret = sscanf(&buffer()[0], "%d", &x);
    if(ret != 1) throw XConvError(__FILE__, __LINE__);
    return x;
}
unsigned int
XCharInterface::toUInt() const throw (XCharInterface::XConvError &) {
    unsigned int x;
    int ret = sscanf(&buffer()[0], "%u", &x);
    if(ret != 1) throw XConvError(__FILE__, __LINE__);
    return x;
}

const std::vector<char> &
XCharInterface::buffer() const {return m_xport->buffer();}

void
XCharInterface::send(const std::string &str) throw (XCommError &)
{
    this->send(str.c_str());
}
void
XCharInterface::send(const char *str) throw (XCommError &)
{
  XScopedLock<XCharInterface> lock(*this);
  try {
      dbgPrint(driver()->getLabel() + " Sending:\"" + dumpCString(str) + "\"");
      m_xport->send(str);
  }
  catch (XCommError &e) {
      e.print(driver()->getLabel() + KAME::i18n(" SendError, because "));
      throw e;
  }
}
void
XCharInterface::sendf(const char *fmt, ...) throw (XInterfaceError &)
{
  va_list ap;
  int buf_size = SNPRINT_BUF_SIZE;
  std::vector<char> buf;
  for(;;) {
      buf.resize(buf_size);
   int ret;
    
      va_start(ap, fmt);
    
      ret = vsnprintf(&buf[0], buf_size, fmt, ap);
    
      va_end(ap);
      
      if(ret < 0) throw XConvError(__FILE__, __LINE__);
      if(ret < buf_size) break;
      
      buf_size *= 2;
  }
  
  this->send(&buf[0]);
}
void
XCharInterface::write(const char *sendbuf, int size) throw (XCommError &)
{
  XScopedLock<XCharInterface> lock(*this);
  try {
      dbgPrint(driver()->getLabel() + QString().sprintf(" Sending %d bytes", size));
      m_xport->write(sendbuf, size);
  }
  catch (XCommError &e) {
      e.print(driver()->getLabel() + KAME::i18n(" SendError, because "));
      throw e;
  }
}
void
XCharInterface::receive() throw (XCommError &)
{
  XScopedLock<XCharInterface> lock(*this);
  try {
      dbgPrint(driver()->getLabel() + " Receiving...");
      m_xport->receive();
      ASSERT(buffer().size());
      dbgPrint(driver()->getLabel() + " Received;\"" + 
        dumpCString((const char*)&buffer()[0]) + "\"");
  }
  catch (XCommError &e) {
        e.print(driver()->getLabel() + KAME::i18n(" ReceiveError, because "));
        throw e;
  }
}
void
XCharInterface::receive(unsigned int length) throw (XCommError &)
{
  XScopedLock<XCharInterface> lock(*this);
  try {
      dbgPrint(driver()->getLabel() + QString(" Receiving %1 bytes...").arg(length));
      m_xport->receive(length);
      dbgPrint(driver()->getLabel() + QString("%1 bytes Received.").arg(buffer().size())); 
  }
  catch (XCommError &e) {
      e.print(driver()->getLabel() + KAME::i18n(" ReceiveError, because "));
      throw e;
  }
}
void
XCharInterface::query(const std::string &str) throw (XCommError &)
{
    query(str.c_str());
}
void
XCharInterface::query(const char *str) throw (XCommError &)
{
  XScopedLock<XCharInterface> lock(*this);
  send(str);
  receive();
}
void
XCharInterface::queryf(const char *fmt, ...) throw (XInterfaceError &)
{
  va_list ap;
  int buf_size = SNPRINT_BUF_SIZE;
  std::vector<char> buf;
  for(;;) {
      buf.resize(buf_size);
   int ret;
    
      va_start(ap, fmt);
    
      ret = vsnprintf(&buf[0], buf_size, fmt, ap);
    
      va_end(ap);
      
      if(ret < 0) throw XConvError(__FILE__, __LINE__);
      if(ret < buf_size) break;
      
      buf_size *= 2;
  }

  this->query(&buf[0]);
}
void
XCharInterface::onSendRequested(const shared_ptr<XValueNodeBase> &)
{
shared_ptr<XPort> port = m_xport;
    if(!port)
       throw XInterfaceError(KAME::i18n("Port is not opened."), __FILE__, __LINE__);
    port->send(m_script_send->to_str().c_str());
}
void
XCharInterface::onQueryRequested(const shared_ptr<XValueNodeBase> &)
{
shared_ptr<XPort> port = m_xport;    
    if(!port)
       throw XInterfaceError(KAME::i18n("Port is not opened."), __FILE__, __LINE__);
    XScopedLock<XCharInterface> lock(*this);
    port->send(m_script_query->to_str().c_str());
    port->receive();
    m_lsnOnQueryRequested->mask();
    m_script_query->value(std::string(&port->buffer()[0]));
    m_lsnOnQueryRequested->unmask();
}

XPort::XPort(XCharInterface *interface)
 : m_pInterface(interface)
{
  m_pInterface->opened()->value(true);
  m_pInterface->device()->setUIEnabled(false);
  m_pInterface->port()->setUIEnabled(false);
  m_pInterface->address()->setUIEnabled(false);
  m_pInterface->baudrate()->setUIEnabled(false);   
}
XPort::~XPort()
{
  m_pInterface->device()->setUIEnabled(true);
  m_pInterface->port()->setUIEnabled(true);
  m_pInterface->address()->setUIEnabled(true);
  m_pInterface->baudrate()->setUIEnabled(true);
  m_pInterface->opened()->value(false);
}
