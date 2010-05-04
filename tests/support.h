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
//Every KAME source must include this header
//---------------------------------------------------------------------------

#ifndef supportH
#define supportH

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>

#ifdef NDEBUG
#define ASSERT(expr)
#define C_ASSERT(expr)
#define DEBUG_XTHREAD 0
#else
#define ASSERT(expr) {if( !(expr)) _my_assert( __FILE__, __LINE__);}
#define C_ASSERT(expr) _my_cassert(sizeof(char [ ( expr ) ? 0 : -1 ]))
inline void _my_cassert(size_t ) {}
int _my_assert(char const*s, int d);
#define DEBUG_XTHREAD 1
#endif

//boost
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::enable_shared_from_this;
using boost::static_pointer_cast;
using boost::dynamic_pointer_cast;
#include <boost/ref.hpp>
using boost::ref;
using boost::reference_wrapper;

#include <math.h>

#if defined __i386__ || defined __i486__ || defined __i586__ || defined __i686__ || defined __x86_64__
struct X86CPUSpec {
	X86CPUSpec();
	unsigned int verSSE;
	bool hasMonitor;
	unsigned int monitorSizeSmallest;
	unsigned int monitorSizeLargest;
};
extern const X86CPUSpec cg_cpuSpec;
#endif

#include <string>
typedef std::string XString;
#include <stdexcept>
//! Base of exception
struct XKameError : public std::runtime_error {
	virtual ~XKameError() throw() {}
	//! errno is read and cleared after a construction
	XKameError(const XString &s, const char *file, int line);
	void print();
	void print(const XString &header);
	static void print(const XString &msg, const char *file, int line, int _errno);
	const XString &msg() const;
	virtual const char* what() const throw();
private:
	const XString m_msg;
	const char *const m_file;
	const int m_line;
	const int m_errno;
};


//! Debug printing.
#define dbgPrint(msg) _dbgPrint(msg, __FILE__, __LINE__)
void
_dbgPrint(const XString &str, const char *file, int line);
//! Global Error Message/Printing.
#define gErrPrint(msg) _gErrPrint(msg, __FILE__, __LINE__)
#define gWarnPrint(msg) _gWarnPrint(msg, __FILE__, __LINE__)
void
_gErrPrint(const XString &str, const char *file, int line);
void
_gWarnPrint(const XString &str, const char *file, int line);

//! If true, use mlockall MCL_FUTURE.
extern bool g_bMLockAlways;
//! If true, use mlock.
extern bool g_bUseMLock;

//---------------------------------------------------------------------------
#endif
