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
#ifndef TRANSACTION_SIGNAL_H
#define TRANSACTION_SIGNAL_H

#include "transaction.h"
#include <deque>
#include "xsignal.h"

namespace Transactional {

template <class XN, typename tArg, typename tArgRef = const tArg &>
struct Event {
	Event(const Snapshot<XN> &s, tArgRef a) :
		shot(s), arg(a) {}
	Snapshot<XN> shot;
	tArg arg;
};

template <class XN, class tClass, typename tArg, typename tArgRef = const tArg &>
struct ListenerRef_ : public XListenerImpl__<Event<XN, tArg, tArgRef> > {
	ListenerRef_(tClass &obj,
		void (tClass::*func)(const Snapshot<XN> &shot, tArgRef),
		XListener::FLAGS flags) :
		XListenerImpl__<Event<XN, tArg, tArgRef> >(flags), m_func(func), m_obj(obj) { }
	virtual void operator() (const Event<XN, tArg, tArgRef> &x) const {
		(m_obj.*m_func)(x.shot, x.arg);
	}
private:
	void (tClass::*const m_func)(const Snapshot<XN> &shot, tArgRef);
	tClass &m_obj;
};
template <class XN, class tClass, typename tArg, typename tArgRef = const tArg &>
struct ListenerWeak_ : public XListenerImpl__<Event<XN, tArg, tArgRef> > {
	ListenerWeak_(const shared_ptr<tClass> &obj,
		void (tClass::*func)(const Snapshot<XN> &shot, tArgRef),
		 XListener::FLAGS flags) :
		 XListenerImpl__<Event<XN, tArg, tArgRef> >(flags), m_func(func), m_obj(obj) { }
	virtual void operator() (const Event<XN, tArg, tArgRef> &x) const {
		if(shared_ptr<tClass> p = m_obj.lock() )
			(p.get()->*m_func)(x.shot, x.arg);
	}
private:
	void (tClass::*const m_func)(const Snapshot<XN> &shot, tArgRef);
	const weak_ptr<tClass> m_obj;
};

template <class XN>
struct Message__ {
	virtual ~Message__() {}
	virtual void talk(const Snapshot<XN> &shot) = 0;
	virtual int unmark(const shared_ptr<XListener> &x) = 0;
};

//! M/M Listener and Talker model
//! \sa XListener, XSignalStore
//! \p tArg: value which will be derivered
//! \p tArgWrapper: copied argument, will be released by GC someday
//!\todo abandon tArgRef, use const tArg&.
template <class XN, typename tArg, typename tArgRef = const tArg &>
class Talker {
public:
	Talker() {}
	Talker(const Talker &x) : m_listeners(x.m_listeners) {}
	virtual ~Talker() {}

	template <class tObj, class tClass>
	shared_ptr<XListener> connect(tObj &obj, void (tClass::*func)(
		const Snapshot<XN> &shot, tArgRef), int flags = 0);
	template <class tObj, class tClass>
	shared_ptr<XListener> connectWeakly(const shared_ptr<tObj> &obj, void (tClass::*func)(
		const Snapshot<XN> &shot, tArgRef), int flags = 0);

	void connect(const shared_ptr<XListener> &);
	void disconnect(const shared_ptr<XListener> &);

	//! Requests a talk to connected listeners.
	//! If a listener is not mainthread model, the listener will be called later.
	//! \param arg passing argument to all listeners
	//! If listener avoids duplication, lock won't be passed to listener.
	virtual Message__<XN>* createMessage(tArgRef arg) const;
	void talk(const Snapshot<XN> &shot, tArgRef arg) const {
		Message m(arg, m_listeners);
		m.talk(shot);
	}

	bool empty() const {return !m_listeners;}
private:
	typedef Event<XN, tArg, tArgRef> Event__;
	typedef XListenerImpl__<Event<XN, tArg, tArgRef> > Listener__;
	typedef std::deque<weak_ptr<Listener__> > ListenerList;
	typedef std::deque<shared_ptr<XListener> > UnmarkedListenerList;
	shared_ptr<ListenerList> m_listeners;

	void connect(const shared_ptr<Listener__> &);

	struct EventWrapper : public XTransaction__ {
		EventWrapper(const shared_ptr<Listener__> &l) :
			XTransaction__(), listener(l) {}
		virtual ~EventWrapper() {}
		const shared_ptr<Listener__> listener;
		virtual bool talkBuffered() = 0;
	};
	struct EventWrapperAllowDup : public EventWrapper {
		EventWrapperAllowDup(const shared_ptr<Listener__> &l, const Event__ &e) :
			EventWrapper(l), event(e) {}
		const Event__ event;
		virtual bool talkBuffered() {
			( *this->listener)(event);
			return false;
		}
	};
	struct EventWrapperAvoidDup : public EventWrapper {
		EventWrapperAvoidDup(const shared_ptr<Listener__> &l) : EventWrapper(l) {}
			virtual bool talkBuffered() {
				bool skip = false;
				if(this->listener->delay_ms()) {
					long elapsed_ms = (timeStamp() - this->registered_time) / 1000uL;
					skip = ((long)this->listener->delay_ms() > elapsed_ms);
				}
				if( !skip) {
					atomic_scoped_ptr<Event__> e;
					e.swap(this->listener->arg);
					ASSERT(e.get());
					( *this->listener)( *e);
				}
				return skip;
			}
	};
protected:
	struct Message : public Message__<XN> {
		Message(tArgRef a, const shared_ptr<ListenerList> &l) : Message__<XN>(), arg(a), listeners(l) {}
		tArg arg;
		shared_ptr<ListenerList> listeners;
		shared_ptr<UnmarkedListenerList> listeners_unmarked;
		virtual void talk(const Snapshot<XN> &shot);
		virtual int unmark(const shared_ptr<XListener> &x) {
			if( !listeners)
				return 0;
			int canceled = 0;
			for(typename ListenerList::const_iterator it = listeners->begin(); it != listeners->end(); it++) {
				if(shared_ptr<Listener__> listener = it->lock()) {
					if(listener == x) {
						if( !listeners_unmarked)
							listeners_unmarked.reset(new UnmarkedListenerList);
						listeners_unmarked->push_back(x);
						++canceled;
					}
				}
			}
			return canceled;
		}
	};
};

template <class XN, typename tArg, typename tArgRef = const tArg &>
class TalkerSingleton : public Talker<XN, tArg, tArgRef> {
public:
	TalkerSingleton() : Talker<XN, tArg, tArgRef>(), m_marked(0) {}
	TalkerSingleton(const TalkerSingleton &x) : Talker<XN, tArg, tArgRef>(x), m_marked(0) {}
	virtual Message__<XN>* createMessage(tArgRef arg) {
		if(m_marked) {
			static_cast<typename Talker<XN, tArg, tArgRef>::Message *>(m_marked)->arg = arg;
			return 0;
		}
		m_marked = Talker<XN, tArg, tArgRef>::createMessage(arg);
		return m_marked;
	}
private:
	Message__<XN> *m_marked;
};

template <class XN, typename tArg, typename tArgRef>
Message__<XN>*
Talker<XN, tArg, tArgRef>::createMessage(tArgRef arg) const {
	if( !m_listeners)
		return 0;
	return new Message(arg, m_listeners);
}

template <class XN, typename tArg, typename tArgRef>
template <class tObj, class tClass>
shared_ptr<XListener>
Talker<XN, tArg, tArgRef>::connect(tObj &obj, void (tClass::*func)(
	const Snapshot<XN> &shot, tArgRef), int flags) {
	shared_ptr<Listener__> listener(
		new ListenerRef_<XN, tClass, tArg, tArgRef>(
			static_cast<tClass&>(obj), func, (XListener::FLAGS)flags) );
	connect(listener);
	return listener;
}

template <class XN, typename tArg, typename tArgRef>
template <class tObj, class tClass>
shared_ptr<XListener>
Talker<XN, tArg, tArgRef>::connectWeakly(const shared_ptr<tObj> &obj,
	void (tClass::*func)(const Snapshot<XN> &shot, tArgRef), int flags) {
	shared_ptr<Listener__> listener(
		new ListenerWeak_<XN, tClass, tArg, tArgRef>(
			static_pointer_cast<tClass>(obj), func, (XListener::FLAGS)flags) );
	connect(listener);
	return listener;
}
template <class XN, typename tArg, typename tArgRef>
void
Talker<XN, tArg, tArgRef>::connect(const shared_ptr<XListener> &lx) {
	shared_ptr<Listener__> listener = dynamic_pointer_cast<Listener__>(lx);
	connect(listener);
}
template <class XN, typename tArg, typename tArgRef>
void
Talker<XN, tArg, tArgRef>::connect(const shared_ptr<Listener__> &lx) {
	shared_ptr<ListenerList> new_list(
		m_listeners ? (new ListenerList( *m_listeners)) : (new ListenerList));
	// clean-up dead listeners.
	for(typename ListenerList::iterator it = new_list->begin(); it != new_list->end();) {
		if( !it->lock())
			it = new_list->erase(it);
		else
			++it;
	}
	new_list->push_back(lx);
	m_listeners = new_list;
}
template <class XN, typename tArg, typename tArgRef>
void
Talker<XN, tArg, tArgRef>::disconnect(const shared_ptr<XListener> &lx) {
	shared_ptr<ListenerList> new_list(
		m_listeners ? (new ListenerList( *m_listeners)) : (new ListenerList));
	for(typename ListenerList::iterator it = new_list->begin(); it != new_list->end();) {
		if(shared_ptr<XListener> listener = it->lock()) {
			// clean dead listeners and matching one.
			if( !listener || (lx == listener)) {
				it = new_list->erase(it);
				continue;
			}
		}
		++it;
	}
	if(new_list->empty()) new_list.reset();
	m_listeners = new_list;
}

template <class XN, typename tArg, typename tArgRef>
void
Talker<XN, tArg, tArgRef>::Message::talk(const Snapshot<XN> &shot) {
	if( !listeners) return;
	//Writing deferred events to event pool.
	for(typename ListenerList::const_iterator it = listeners->begin(); it != listeners->end(); it++) {
		if(shared_ptr<Listener__> listener = it->lock()) {
			if(listeners_unmarked &&
				(std::find(listeners_unmarked->begin(), listeners_unmarked->end(), listener) != listeners_unmarked->end()))
				continue;
			if(listener->m_flags & XListener::FLAG_MAIN_THREAD_CALL) {
				if(listener->m_flags & XListener::FLAG_AVOID_DUP) {
					atomic_scoped_ptr<Event__> newevent(new Event__(shot, arg) );
					newevent.swap(listener->arg);
					if( !newevent.get())
						registerTransactionList(new EventWrapperAvoidDup(listener));
				}
				else {
					if(isMainThread()) {
						try {
							( *listener)(Event__(shot, arg));
						}
						catch (XKameError &e) {
							e.print();
						}
					}
					else {
						registerTransactionList(new EventWrapperAllowDup(listener, Event__(shot, arg)));
					}
				}
			}
		}
	}
	//Immediate events.
	for(typename ListenerList::const_iterator it = listeners->begin(); it != listeners->end(); it++) {
		if(shared_ptr<Listener__> listener = it->lock()) {
			if(listeners_unmarked &&
				(std::find(listeners_unmarked->begin(), listeners_unmarked->end(), listener) != listeners_unmarked->end()))
				continue;
			if( !(listener->m_flags & XListener::FLAG_MAIN_THREAD_CALL)) {
				try {
					( *listener)(Event__(shot, arg));
				}
				catch (XKameError &e) {
					e.print();
				}
			}
		}
	}
}

} //namespace Transactional

#endif /*TRANSACTION_SIGNAL_H*/
