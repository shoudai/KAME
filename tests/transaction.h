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
#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "support.h"
#include "atomic_smart_ptr.h"

//! Example 1\n
//! shared_ptr<Subscriber> ss1 = monitor1->monitorData();\n
//! sleep(1);\n
//! if(Snapshot<NodeA> shot1(monitor1)) { // checking consistency (i.e. requiring at least one transaction).\n
//! double x = shot1[node1]; //implicit conversion defined in Node1::Payload.\n
//! double y = shot1[node1].y(); }\n
//!\n
//! Example 2\n
//! double x = *node1; // for an immediate access.\n
//!\n
//! Example 3\n
//! for(;;) { Transaction<NodeA> tr1(monitor1);\n
//! tr1[node1] = tr1[node1] * 2.0;\n
//! if(tr1.commit()) break;}\n
//! \n
//! Example 5\n
//! //Obsolete, for backward compatibility.\n
//! monitor1.readLock(); // or use lock(), a snapshot will be copied to TLS.\n
//! double x = (*node1).y();\n
//! monitor1.readUnock(); // or use unlock()\n
//! \n
//! Example 6\n
//! //Obsolete, for backward compatibility.\n
//! monitor1.writeLock(); // a transaction will be prepared in TLS.\n
//! (*node1).value(1.0);\n
//! monitor1.writeUnock(); // commit the transaction.\n

//! Watch point for transactional memory access.\n
//! The list of the pointers to data is atomically read/written.
/*!
 * Criteria during the transaction:\n
 * 	a) When a conflicting writing to the same node is detected, the \a commit() will fail.
 * 	b) Writing to listed nodes not by the this context but by another thread will be included in the final result.
 */
//! A node which carries data sets for itself and subnodes.
//! Transactional accesses will be made on the top branch in the tree.
//! \sa Snapshot, Transaction, XNode

#include <vector>
#include <map>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>
#include "atomic.h"

namespace Transactional {

template <class XN>
class Snapshot;
template <class XN>
class Transaction;

template <class XN>
class Node {
public:
	Node();
	virtual ~Node();

	class Packet;
	struct PacketList;
	struct NodeList : public std::vector<shared_ptr<XN> > {
		NodeList() : std::vector<shared_ptr<XN> >(), m_superNodeList(), m_index(0), m_serial(-1) {}
		//! Reverse address to the super nodes in the bundle.
		weak_ptr<NodeList> m_superNodeList;
		int m_index;
		//! Serial number of the transaction.
		int64_t m_serial;
		//! finds packet for this.
		//! \arg copy_branch If true, all packets between the root and this will be copy-constructed unless the serial numbers are the same.
		//! \sa Node::reverseLookup().
		inline local_shared_ptr<Packet> *reverseLookup(local_shared_ptr<Packet> &packet, bool copy_branch, int tr_serial);
	private:
	};
	struct PacketList : public std::vector<local_shared_ptr<Packet> > {
		shared_ptr<NodeList> m_subnodes;
		PacketList() : std::vector<local_shared_ptr<Packet> >(), m_serial(-1) {}
		//! Serial number of the transaction.
		int64_t m_serial;
	};

	typedef typename NodeList::iterator iterator;
	typedef typename NodeList::const_iterator const_iterator;

	//! Data holder.
	struct Payload {
		Payload(Node &node) : m_node(&node), m_serial(-1) {ASSERT(&node);}
		virtual ~Payload() {}
	private:
		Payload();
		friend class Node;
		friend class Packet;
		template <class >
		friend class Transaction;
		Node &node() {return *m_node;}
		const Node &node() const {return *m_node;}
		Node *m_node;
		int64_t m_serial;
	};

	struct Packet {
		Packet(const shared_ptr<atomic_shared_ptr<Packet> > &branchpoint);
		~Packet();
	private:
		Packet();
	public:
		//! \return If true, the content is a snapshot, and is up-to-date for the watchpoint.\n
		//! The subnodes must not have their own payloads.
		//! If false, the content may be out-of-date and ones should fetch those on subnodes.
		//! \sa isHere().
		bool isBundled() const {return (m_state & PACKET_BUNDLE_STATE) == PACKET_BUNDLED;}
		//! \sa NullPacket.
		bool isHere() const {return (m_state & PACKET_BUNDLE_STATE) != PACKET_NOT_HERE;}
		void setBundled(bool x) {m_state = (m_state & ~PACKET_BUNDLE_STATE) |
			(x ? PACKET_BUNDLED : PACKET_UNBUNDLED);
		}
		int size() const {return subpackets() ? subpackets()->size() : 0;}
		shared_ptr<Payload> &payload() {return m_payload;}
		const shared_ptr<Payload> &payload() const {return m_payload;}
		shared_ptr<NodeList> &subnodes() {return subpackets()->m_subnodes;}
		shared_ptr<PacketList> &subpackets() {return m_subpackets;}
		const shared_ptr<NodeList> &subnodes() const {return subpackets()->m_subnodes;}
		const shared_ptr<PacketList> &subpackets() const {return m_subpackets;}

		shared_ptr<atomic_shared_ptr<Packet> > branchpoint() {return m_branchpoint.lock();}
		//! points to the node.
		Node &node() {return payload()->node();}
		//! points to the node.
		const Node &node() const {return payload()->node();}

		void print();
	protected:
	private:
		friend class Node;
		friend class Snapshot<XN>;
		friend class Transaction<XN>;
		int m_state;
		enum STATE {
			PACKET_BUNDLE_STATE = 0xf,
			PACKET_UNBUNDLED = 0x1, PACKET_BUNDLED = 0x2, PACKET_NOT_HERE = 0x3};
		shared_ptr<Payload> m_payload;
		shared_ptr<PacketList> m_subpackets;
		weak_ptr<atomic_shared_ptr<Packet> > m_branchpoint;
		//! Serial number of the transaction.
		int64_t m_serial;
	};
	struct NullPacket : public Packet {
		NullPacket(const shared_ptr<atomic_shared_ptr<Packet> > &branchpoint) : Packet(branchpoint) {
			this->m_state = Node<XN>::Packet::PACKET_NOT_HERE;
		}
	private:
		NullPacket();
	};
	bool insert(const Snapshot<XN> &snapshot, const shared_ptr<XN> &var);
	void insert(const shared_ptr<XN> &var);
	bool release(const Snapshot<XN> &snapshot, const shared_ptr<XN> &var);
	void release(const shared_ptr<XN> &var);
	void releaseAll();
	bool swap(const Snapshot<XN> &snapshot, const shared_ptr<XN> &x, const shared_ptr<XN> &y);
	void swap(const shared_ptr<XN> &x, const shared_ptr<XN> &y);
private:
	friend class Snapshot<XN>;
	friend class Transaction<XN>;
	void snapshot(local_shared_ptr<Packet> &target) const;
	static inline bool trySnapshotSuper(atomic_shared_ptr<Packet> &branchpoint, local_shared_ptr<Packet> &target);
	bool commit(const local_shared_ptr<Packet> &oldpacket, local_shared_ptr<Packet> &newpacket);
	enum BundledStatus {BUNDLE_SUCCESS, BUNDLE_DISTURBED};
	BundledStatus bundle(local_shared_ptr<Packet> &target);
	enum UnbundledStatus {UNBUNDLE_W_NEW_SUBVALUE, UNBUNDLE_W_NEW_VALUES,
		UNBUNDLE_SUBVALUE_HAS_CHANGED,
		UNBUNDLE_SUCCESS, UNBUNDLE_DISTURBED};
	static UnbundledStatus unbundle(atomic_shared_ptr<Packet> &branchpoint,
		atomic_shared_ptr<Packet> &subbranchpoint, const local_shared_ptr<Packet> &nullpacket,
		const local_shared_ptr<Packet> *oldsubpacket = NULL, local_shared_ptr<Packet> *newsubpacket = NULL,
		const local_shared_ptr<Packet> *oldsuperpacket = NULL, const local_shared_ptr<Packet> *newsuperpacket = NULL);
	shared_ptr<atomic_shared_ptr<Packet> > m_packet;

	struct LookupHint {
		weak_ptr<NodeList> m_superNodeList;
		//! The node is expected to be found at the (super_node_list)[\a m_index], unless the list has altered.
		int m_index;
	};
	//! A clue for reverseLookup().
	mutable atomic_shared_ptr<LookupHint> m_lookupHint;
	//! finds the packet for this node in the (un)bundled \a packet.
	//! \arg packet The bundled packet.
	//! \arg copy_branch If ture, new packets and packet lists will be copy-created for writing.
	//! \arg tr_serial The serial number associated with the transaction.
	local_shared_ptr<Packet> &reverseLookup(local_shared_ptr<Packet> &packet, bool copy_branch, int tr_serial = 0) const;
	const local_shared_ptr<Packet> &reverseLookup(const local_shared_ptr<Packet> &packet) const {
		reverseLookup(const_cast<local_shared_ptr<Packet> &>(packet), false);
	}
	//! finds this node in the (un)bundled \a packet.
	//! \arg hint The information for reverseLookup() will be returned.
	bool forwardLookup(const local_shared_ptr<Packet> &packet, local_shared_ptr<LookupHint> &hint) const;
	static void recreateNodeTree(local_shared_ptr<Packet> &packet);
protected:
	void initPayload(Payload *payload);
};

//! This class takes a snapshot for a monitored data set.
template <class XN>
class Snapshot {
public:
	Snapshot(const Snapshot&x) : m_packet(x.m_packet) {}
	Snapshot(const Transaction<XN>&x);
	explicit Snapshot(const Node<XN>&node) {
		node.snapshot(m_packet);
		ASSERT(m_packet->isBundled());
	}
	virtual ~Snapshot() {}

	template <class T>
	const typename T::Payload &operator[](const shared_ptr<T> &node) const {
		return operator[](const_cast<const T&>(*node));
	}
	template <class T>
	const typename T::Payload &operator[](const T &node) const {
		if(&node == &m_packet->node())
			return dynamic_cast<typename T::Payload&>(*m_packet->payload());
		return dynamic_cast<const typename T::Payload&>(*node.reverseLookup(m_packet)->payload());
	}
	int size() const {return m_packet->size();}
	const shared_ptr<const typename Node<XN>::NodeList> list() const {
		if( ! size())
			return shared_ptr<typename Node<XN>::NodeList>();
		return m_packet->subnodes();
	}
	int size(const shared_ptr<Node<XN> > &node) const {
		return node->reverseLookup(m_packet)->size();
	}
	shared_ptr<const typename Node<XN>::NodeList> list(const shared_ptr<Node<XN> > &node) const {
		local_shared_ptr<typename Node<XN>::Packet> const &packet(node->reverseLookup(m_packet));
		if( ! packet->size() )
			return shared_ptr<typename Node<XN>::NodeList>();
		return packet->subnodes();
	}
	void print() {
		m_packet->print();
	}
protected:
	friend class Node<XN>;
	//! The snapshot.
	local_shared_ptr<typename Node<XN>::Packet> m_packet;
};

//! Transactional writing for a monitored data set.
//! The revision will be committed implicitly on leaving the scope.
template <class XN>
class Transaction : public Snapshot<XN> {
public:
	Transaction(const Transaction &tr) : Snapshot<XN>(tr), m_oldpacket(tr.m_oldpacket) {}
	//! Be sure to the persistence of the \a node.
	explicit Transaction(const Node<XN>&node) :
		Snapshot<XN>(node), m_oldpacket(this->m_packet) {
		ASSERT(&this->m_packet->node() == &node);
		for(;;) {
			m_serial = s_serial;
			if(s_serial.compareAndSet(m_serial, m_serial + 1))
				break;
		}
		m_serial++;
	}
	virtual ~Transaction() {}
	//! Explicitly commits.
	bool commit() {
		if(!isModified())
			return true;
		return this->m_packet->node().commit(m_oldpacket, this->m_packet);
	}
	//! Explicitly commits.
	bool commitOrNext() {
		if(this->m_packet->node().commit(m_oldpacket, this->m_packet))
			return true;
		++(*this);
		return false;
	}

	bool isModified() const {
		return (this->m_packet->m_serial == m_serial);
	}

	Transaction &operator++() {
		this->m_packet->node().snapshot(m_oldpacket);
		this->m_packet = m_oldpacket;
		return *this;
	}

	template <class T>
	typename T::Payload &operator[](const shared_ptr<T> &node) {
		return operator[](*node);
	}
	template <class T>
	typename T::Payload &operator[](T &node) {
		shared_ptr<typename Node<XN>::Payload> *payload;
		if(&node == &this->m_packet->node()) {
			if(this->m_packet->m_serial != m_serial) {
				this->m_packet.reset(new typename Node<XN>::Packet(*m_oldpacket));
				this->m_packet->m_serial = m_serial;
			}
			payload = &this->m_packet->payload();
		}
		else {
			payload = &node.reverseLookup(this->m_packet, true, m_serial)->payload();
		}
		if((*payload)->m_serial != m_serial) {
			typename T::Payload *newpayload =
				new typename T::Payload(dynamic_cast<typename T::Payload&>(**payload));
			payload->reset(newpayload);
			(*payload)->m_serial = m_serial;
			return *newpayload;
		}
		return dynamic_cast<typename T::Payload&>(**payload);
	}
private:
	friend class Node<XN>;
	shared_ptr<typename Node<XN>::PacketList> &subpackets() {return this->m_packet->subpackets();}
	const shared_ptr<typename Node<XN>::PacketList> &subpackets() const {return this->m_packet->subpackets();}
	local_shared_ptr<typename Node<XN>::Packet> m_oldpacket;
	int64_t m_serial;
	static atomic<int> s_serial;
};

template <class XN>
inline Snapshot<XN>::Snapshot(const Transaction<XN>&x) : m_packet(x.m_packet) {}

template <class T, class XN>
struct _implicitReader : public Snapshot<XN> {
	_implicitReader(const T &node) : Snapshot<XN>(node) {
	}
	const typename T::Payload *operator->() const {
		return &dynamic_cast<typename T::Payload&>(*this->m_packet->payload());
	}
	template <class X>
	operator X() const {(X)dynamic_cast<typename T::Payload&>(*this->m_packet->payload());}
};

} //namespace Transactional

#endif /*TRANSACTION_H*/