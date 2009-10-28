/***************************************************************************
		Copyright (C) 2002-2009 Kentaro Kitagawa
		                   kitag@issp.u-tokyo.ac.jp

		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.

		You should have received a copy of the GNU Library General 
		Public License and a list of authors along with this program; 
		see the files COPYING and AUTHORS.
 ***************************************************************************/
#ifndef MONITOR_H
#define MONITOR_H

#include "atomic_smart_ptr.h"

//! Example 1\n
//! shared_ptr<Subscriber> ss1 = monitor1->monitorData();\n
//! sleep(1);\n
//! if(Snapshot<MonitorA> shot1(monitor1)) { // checking consistency (i.e. requiring at least one transaction).\n
//! double x = shot1[node1]; //implicit conversion defined in Node1::Baggage.\n
//! double y = shot1[node1]->y(); }\n
//!\n
//! Example 2\n
//! double x = *node1; // for an immediate access, same as (double)(const Node1::Baggage&)(*node1)\n
//!\n
//! Example 3\n
//! { Transaction<MonitorA> tr1(monitor1);\n
//! tr1[node1] = tr1[node1] * 2.0; }\n
//! \n
//! Example 4\n
//! node1->value(1.0); // for an immediate access.\n
//! \n
//! Example 5\n
//! //Obsolete, for backward compatibility.\n
//! monitor1.readLock(); // or use lock(), a snapshot will be copied to TLS.\n
//! double x = node1->y();\n
//! monitor1.readUnock(); // or use unlock()\n
//! \n
//! Example 6\n
//! //Obsolete, for backward compatibility.\n
//! monitor1.writeLock(); // a transaction will be prepared in TLS.\n
//! node1->value(1.0);\n
//! monitor1.writeUnock(); // commit the transaction.\n

//! Watch point for transactional memory access.\n
//! The list of the pointers to data is atomically read/written.
/*!
 * Criteria during the transaction:\n
 * 	a) When a conflicting writing to the same node is detected, the \a commit() will fail.
 * 	b) Writing to listed nodes not by the this context but by another thread will be included in the final result.
 */
//! \sa Metamonitor, Snapshot, Transaction, XNode
class Monitor {
public:
	Monitor() {}
	virtual ~Monitor() {}

	//! Data holder.
	struct Baggage {
		virtual ~Baggage();
	protected:
	private:
		shared_ptr<NodeData> m_data;
		shared_ptr<NodeList> m_children;
		shared_ptr<BaggageList> m_subBaggages;
		int m_tag;
		int m_flags;
		int m_serial;
		enum FLAG {BAGGAGE_MODIFIED = 1, BAGGAGE_RESERVED = 2, BAGGAGE_DEPEND = 4, BAGGAGE_SUB = 8};
		weak_ptr<XNode> m_parent;
	};

	class writer {
	public:
		writer(XNode &node) : m_node(node), m_old_baggage(node.m_baggage),
			m_new_baggage(m_old_baggage ? (new Baggage(*m_old_baggage)) : new Baggage()) {
		}
		Baggage &operator[](const shared_ptr<XNode> &x) {
			return resolve(x);
		}
		Baggage &resolve(const shared_ptr<XNode>&);
		bool checkConflict(const Baggage &stored_baggage) {
			if(m_new_baggage->m_serial != m_old_baggage->m_serial) {
				if(m_old_baggage->m_serial != stored_baggage->m_serial)
					return false;
			}
			else {
				m_new_baggage->m_data = stored_baggage->m_data;
				m_new_baggage->m_children = stored_baggage->m_children;
			}
			if(children()) {
				for(unsigned int i = 0; i < children().size(); i++) {
					if(m_subBaggages[i]) {
						if(!checkConflict(stored_baggage))
							return false;
					}
				}
			}
			return true;
		}
		bool commit() {
			for(;;) {
				if(m_new_baggage.compareAndSet(m_old_baggage, m_root.m_baggage))
					return true;
				atomic_shared_ptr<Baggage> stored_baggage(m_root.m_baggage);
				if(!checkConflict(stored_baggage))
					return false;
				m_old_baggage = stored_baggage;
			}
		}
	private:
		XNode &m_root;
		atomic_shared_ptr<Baggage> m_old_baggage, m_new_baggage;
	};
	typedef typename transactional<Baggage>::reader reader;
	template <class T>
	operator T::Baggage&() {return dynamic_cast<T::Baggage&>(*m_baggage);}

	reader _baggage() const {return m_baggage;}

private:
	transactional<Baggage> m_baggage;
};

class Metamonitor : public Monitor {
public:
	struct Packet : public Monitor::Packet {
		virtual shared_ptr<DataMap> dataMap() {return m_dataMap;}
		virtual Packet &resolve(const Monitor &monitor) {
			return dataMap()->find(monitor).second;
		}
		typedef std::deque<Baggage> BaggageList;
		BaggageList m_baggages;
		shared_ptr<MonitorList> m_monitorList;
		shared_ptr<DataMap> m_dataMap;
	};
private:
};


//! This class takes a snapshot for a monitored data set.
template <class M>
class Snapshot {
public:
	Snapshot(const Snapshot&x) : m_monitor(x.m_monitor), m_baggage(x.m_baggage) {}
	Snapshot(const shared_ptr<M>&mon) : m_monitor(mon), m_baggage(mon->_baggage()) {}
	~Snapshot() {}

	template <class T>
	const T::Baggage &operator[](const shared_ptr<T> &monitor) const {
		return dynamic_cast<const T::Baggage&>(monitor->resolve(*this));}
private:
	//! The snapshot.
	const shared_ptr<M> m_monitor;
	const atomic_shared_ptr<Baggage> m_baggage;
};

//! Transactional writing for a monitored data set.
//! The revision will be committed implicitly on leaving the scope.
class Transaction : public Snapshot {
public:
	Transaction(const Transaction&x) : Snapshot(x), m_newdata(x.m_newdata) {}
	Transaction(const shared_ptr<Monitor>&mon) : 
		Snapshot(mon), m_newdata(new Monitor::DataList(data())) {}
	~Transaction() {}
	//! Explicit commit.
	void commit();
	//! Abandon revision.
	void abort();

	template <class T>
	struct accessor {
		accessor(const shared_ptr<T> &t) : m_var(t) {}
		template <class X>
		operator X() const {return (X)*m_var;}
		T &operator->() const {return *m_var;}
		template <class X>
		accessor &operator=(const X &x) {m_var->value(x);}
	private:
		shared_ptr<T> m_var;
	};
	//! For implicit casting.
	template <class T>
	const accessor &operator[](const shared_ptr<T> &t) const {
		return accessor<T>(dynamic_pointer_cast<T>(m_newpacket->dataMap().find(t.ptr())->second->resolve()));}
private:
	shared_ptr<Monitor::Packet> m_newpacket;
};

class XMonitor : public Monitor {
};
class XGroupMonitor : public XMonitor {
};
template <typename T>
class Transactional : public _transactional {
	struct Data : public Metadata {
		shared_ptr<T> var;
	};
	atomic_shared_ptr<Data> m_data;
	void _commit(const shared_ptr<T>&t, const packed* = NULL);
	shared_ptr<const T> read(const Snapshot &shot) const;
};

template <typename T>
void Transactional::_commit(const shared_ptr<T>&t, const snapshot*) {
	shared_ptr<WatcherList> new_list;
	shared_ptr<Data> newone(new Data(*m_data));
	newone->var = t;
	for(int j = 0; j < newone->watchers.size(); j++) {
		Watcher watcher = newone->watchers->at(j);
		shared_ptr<Monitor> mon = watcher.monitor.lock();
		if(!mon) {
			//Remove from the list;
			if(!new_list)
				new_list.reset(new WatcherList(*newone->watchers));
			new_list.erase(std::remove(new_list.begin(), new_list.end(), watcher), new_list.end());
			continue;
		}
		if(mon->m_bActive) {
			if(packed && (packed->m_monitor == mon)) {
			//Write in the working set.
				packed->m_newdata->at(watcher.index) = newone;
			}
			else {
				for(;;) {
					atomic_shared_ptr<Monitor::TransactionalList> oldrecord(mon->m_watchpoint);
					atomic_shared_ptr<Monitor::TransactionalList> newrecord(new Monitor::TransactionalList(*mon->m_watchpoint));
					ASSERT(watcher.index < oldrecord->size());
					newrecord->at(watcher.index) = newone;
					if(mon->m_watchpoint.compareAndSet(oldrecord, newrecord))
						break;
				}
			}
		}
	}
	for(;;) {
		atomic_shared_ptr<Data> oldone(m_data);
		atomic_shared_ptr<Data> newone2(new Data(*m_data));
		newone2->var = t;
		if(new_list && (newone->watchers == newone2->watchers))
			newone2->watchers = new_list;
		if(m_data.compareAndSet(oldone, newone2))
			break;
	}
}
template <typename T>
void Transactional::write(const PackedWrite &packed, const shared_ptr<T>&t) {
	_commit(t, &packed);
}

template <typename T>
shared_ptr<const T> Transactional::read(const Snapshot &shot) const {
}

class XNode {
	typedef std::deque<shared_ptr<XNode> > NodeList;
	struct Property {
		int flags;
		NodeList children;
		weak_ptr<XNode> parent;
	};
	Transactional<Property> m_property;
};

#endif /*MONITOR_H*/