#ifndef INTERFACE_H_
#define INTERFACE_H_

#include "xnode.h"
#include "xlistnode.h"
#include "xitemnode.h"
#include <vector>

class XDriver;

//! virtual class for communication devices.
//! \sa XCharInterface
class XInterface : public XNode
{
 XNODE_OBJECT
protected:
 XInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver);
public:
 virtual ~XInterface() {}
 
 struct XInterfaceError : public XKameError {
    XInterfaceError(const QString &msg, const char *file, int line);
 };
 struct XConvError : public XInterfaceError {
    XConvError(const char *file, int line);
 };
 struct XCommError : public XInterfaceError {
    XCommError(const QString &, const char *file, int line);
 };
 struct XOpenInterfaceError : public XInterfaceError {
    XOpenInterfaceError(const char *file, int line);
 };
 
 shared_ptr<XDriver> driver() const {return m_driver.lock();}
 
 //! type of interface or driver.
 const shared_ptr<XComboNode> &device() const {return m_device;}
  //! port number or device name.
 const shared_ptr<XStringNode> &port() const {return m_port;}
  //! e.g. GPIB address.
 const shared_ptr<XUIntNode> &address() const {return m_address;}
  //! e.g. Serial port baud rate.
 const shared_ptr<XUIntNode> &baudrate() const {return m_baudrate;}
 //! True if interface is opened.
 const shared_ptr<XBoolNode> &opened() const {return m_opened;}

  void lock() {m_mutex.lock();}
  void unlock() {m_mutex.unlock();}
  bool isLocked() const {return m_mutex.isLockedByCurrentThread();}

  XRecursiveMutex &mutex() {return m_mutex;}
  
  virtual void open() throw (XInterfaceError &) = 0;
  //! This can be called even if has already closed.
  virtual void close() = 0;
    
  virtual bool isOpened() const = 0;
private:
  weak_ptr<XDriver> m_driver;

  shared_ptr<XComboNode> m_device;
  shared_ptr<XStringNode> m_port;
  shared_ptr<XUIntNode> m_address;
  shared_ptr<XUIntNode> m_baudrate;
  shared_ptr<XBoolNode> m_opened;
      
  XRecursiveMutex m_mutex;
};


class XInterfaceList : public XAliasListNode<XInterface>
{
 XNODE_OBJECT
protected:
    XInterfaceList(const char *name, bool runtime) : XAliasListNode<XInterface>(name, runtime) {}
};

#endif /*INTERFACE_H_*/
