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
#include "transaction.h"
#include <vector>

atomic<int> Transaction::s_serial = 0;

Node::Packet::Packet(const shared_ptr<atomic_shared_ptr<Packet> > &branchpoint) :
	m_state(PACKET_BUNDLED), m_payload(),
	m_branchpoint(branchpoint), m_serial(-1) {
}
Node::Packet::~Packet() {
}
void
Node::Packet::print() {
	printf("Packet: ");
	printf("Bundler:%llx, ", (uintptr_t)branchpoint().get());
	if( ! isHere())
		printf("Not here, ");
	else {
		printf("Node:%llx, ", (uintptr_t)&node());

		if(isBundled())
			printf("Bundled, ");
		if(size()) {
			printf("%d subnodes : [ ", size());
			for(unsigned int i = 0; i < size(); i++) {
				if(subpackets()->at(i)) {
					subpackets()->at(i)->print();
				}
			}
			printf("]");
		}
	}
	printf("\n");
}

Node::Node() : m_packet(new atomic_shared_ptr<Packet>(
	new Packet(shared_ptr<atomic_shared_ptr<Packet> >()))) {
	initPayload(new Payload(*this));
}
Node::~Node() {
	releaseAll();
}
void
Node::initPayload(Payload *payload) {
	local_shared_ptr<Packet>(*m_packet)->payload().reset(payload);
}
void
Node::recreateNodeTree(local_shared_ptr<Packet> &packet) {
	unsigned int idx = 0;
	packet.reset(new Packet(*packet));
	packet->subpackets().reset(packet->size() ? (new PacketList(*packet->subpackets())) : (new PacketList));
	packet->subnodes().reset(packet->size() ? (new NodeList(*packet->subnodes())) : (new NodeList));
	for(PacketList::iterator pit = packet->subpackets()->begin(); pit != packet->subpackets()->end();) {
		if((*pit)->size()) {
			pit->reset(new Packet(**pit));
		}
		if((*pit)->size()) {
			(*pit)->subpackets().reset(new PacketList(*(*pit)->subpackets()));
			(*pit)->subnodes().reset(new NodeList(*(*pit)->subnodes()));
			(*pit)->subnodes()->m_superNodeList = packet->subnodes();
			ASSERT((*pit)->subnodes()->m_index == idx);
			recreateNodeTree(*pit);
		}
		++pit;
		++idx;
	}
}
void
Node::insert(const shared_ptr<Node> &var) {
	for(;;) {
		Snapshot shot(*this);
		if(insert(shot, var))
			break;
	}
}
bool
Node::insert(const Snapshot &snapshot, const shared_ptr<Node> &var) {
	local_shared_ptr<Packet> packet(snapshot.m_packet);
	recreateNodeTree(packet);
	packet->subpackets()->resize(packet->size() + 1);
	ASSERT(packet->subnodes());
	packet->subnodes()->push_back(var);
	ASSERT(packet->subpackets()->size() == packet->subnodes()->size());
	packet->setBundled(false);
//		printf("i");
	if(commit(snapshot.m_packet, packet)) {
		local_shared_ptr<LookupHint> hint(new LookupHint);
		hint->m_index = packet->size() - 1;
		hint->m_superNodeList = packet->subnodes();
		var->m_lookupHint = hint;
		return true;
	}
	return false;
}
void
Node::release(const shared_ptr<Node> &var) {
	for(;;) {
		Snapshot shot(*this);
		if(release(shot, var))
			break;
	}
}
bool
Node::release(const Snapshot &snapshot, const shared_ptr<Node> &var) {
	local_shared_ptr<Packet> packet(snapshot.m_packet);
	local_shared_ptr<Node::Packet> oldsubpacket(
		var->reverseLookup(packet));
	recreateNodeTree(packet);
	local_shared_ptr<Node::Packet> newsubpacket;

	NodeList::iterator nit = packet->subnodes()->begin();
	for(PacketList::iterator pit = packet->subpackets()->begin(); pit != packet->subpackets()->end();) {
		if(nit->get() == &*var) {
			if((*pit)->size()) {
				(*pit)->subnodes()->m_superNodeList.reset();
			}
			else {
				pit->reset(new Packet(**pit));
			}
			newsubpacket = *pit;
			(*pit)->m_branchpoint.reset();
			pit = packet->subpackets()->erase(pit);
			nit = packet->subnodes()->erase(nit);
		}
		else {
			++nit;
			++pit;
		}
	}
	ASSERT(newsubpacket);
	ASSERT( ! newsubpacket->branchpoint() );

	if( ! packet->size()) {
		packet->subpackets().reset();
		ASSERT(packet->isBundled());
	}
	else {
		packet->setBundled(false);
	}
	local_shared_ptr<Packet> nullpacket(*var->m_packet);
	if(nullpacket->isHere())
		return false;
//		printf("r");
	Node::UnbundledStatus ret = unbundle(*m_packet, *var->m_packet,
		nullpacket, &oldsubpacket, &newsubpacket, &snapshot.m_packet, &packet);
	if(ret == UNBUNDLE_W_NEW_VALUES) {
//			printf("%d", (int)packet->size());
		var->m_lookupHint.reset();
		return true;
	}
	return false;
}
void
Node::releaseAll() {
	for(;;) {
		Snapshot shot(*this);
		if( ! shot.size())
			break;
		shared_ptr<const Node::NodeList> list(shot.list());
		release(shot, list->front());
	}
}
bool
Node::swap(const Snapshot &snapshot, const shared_ptr<Node> &x, const shared_ptr<Node> &y) {
}

inline local_shared_ptr<Node::Packet>*
Node::NodeList::reverseLookup(local_shared_ptr<Packet> &packet, bool copy_branch, int tr_serial) {
	local_shared_ptr<Node::Packet> *foundpacket;
	if(packet->subnodes().get() == this) {
		foundpacket = &packet;
	}
	else {
		shared_ptr<NodeList> superlist = m_superNodeList.lock();
		if( ! superlist)
			return NULL;
		foundpacket =
			superlist->reverseLookup(packet, copy_branch, tr_serial);
		if( ! foundpacket)
			return NULL;
		if((*foundpacket)->size() <= m_index)
			return NULL;
		foundpacket = &(*foundpacket)->subpackets()->at(m_index);
		if((*foundpacket)->subnodes().get() != this)
			return NULL;
		ASSERT((*foundpacket)->isBundled());
	}
	if(copy_branch) {
		if((*foundpacket)->subpackets()->m_serial != tr_serial) {
			if((*foundpacket)->m_serial != tr_serial) {
				foundpacket->reset(new Packet(**foundpacket));
				(*foundpacket)->m_serial = tr_serial;
			}
			(*foundpacket)->subpackets().reset(new PacketList(*(*foundpacket)->subpackets()));
			(*foundpacket)->subpackets()->m_serial = tr_serial;
		}
		ASSERT((*foundpacket)->m_serial == tr_serial);
	}
	return foundpacket;
}
local_shared_ptr<Node::Packet>&
Node::reverseLookup(local_shared_ptr<Packet> &packet, bool copy_branch, int tr_serial) const {
	ASSERT(packet->size());
	local_shared_ptr<LookupHint> hint(m_lookupHint);
	for(int i = 0;; ++i) {
		ASSERT(i < 2);
		if(hint) {
			shared_ptr<NodeList> supernodelist = hint->m_superNodeList.lock();
			if(supernodelist &&
				((hint->m_index < supernodelist->size()) &&
					(supernodelist->at(hint->m_index).get() == this))) {
				local_shared_ptr<Node::Packet>* superpacket = supernodelist->reverseLookup(packet, copy_branch, tr_serial);
				if(superpacket &&
					((*superpacket)->size() > hint->m_index) ) {
					local_shared_ptr<Node::Packet> &foundpacket((*superpacket)->subpackets()->at(hint->m_index));
					if(&foundpacket->node() == this) {
						if(copy_branch && (foundpacket->m_serial != tr_serial)) {
							foundpacket.reset(new Packet(*foundpacket));
							foundpacket->m_serial = tr_serial;
						}
						ASSERT(foundpacket->isBundled());
//						printf("#");
						return foundpacket;
					}
				}
			}
		}
//		printf("!");
		bool ret = forwardLookup(packet, hint);
		ASSERT(ret);
		m_lookupHint = hint;
	}
}
bool
Node::forwardLookup(const local_shared_ptr<Packet> &packet, local_shared_ptr<LookupHint> &hint) const {
	if( ! packet->subpackets())
		return false;
	for(unsigned int i = 0; i < packet->subnodes()->size(); i++) {
		if(packet->subnodes()->at(i).get() == this) {
			hint.reset(new LookupHint);
			hint->m_index = i;
			hint->m_superNodeList = packet->subnodes();
			return true;
		}
	}
	for(unsigned int i = 0; i < packet->subnodes()->size(); i++) {
		if(forwardLookup(packet->subpackets()->at(i), hint)) {
			return true;
		}
	}
	return false;
}

void
Node::snapshot(local_shared_ptr<Packet> &target) const {
	for(;;) {
		target = *m_packet;
		if(target->isBundled())
			return;
		if( ! target->isHere()) {
			shared_ptr<atomic_shared_ptr<Packet> > branchpoint(m_packet);
			if(trySnapshotSuper(*branchpoint, target)) {
				if( ! target->size())
					continue;
				target = const_cast<Node*>(this)->reverseLookup(target);
				ASSERT(target->isBundled());
				return;
			}
			continue;
		}
		BundledStatus status = const_cast<Node*>(this)->bundle(target);
		if(status == BUNDLE_SUCCESS)
			return;
	}
}
inline bool
Node::trySnapshotSuper(atomic_shared_ptr<Packet> &branchpoint, local_shared_ptr<Packet> &target) {
	local_shared_ptr<Packet> oldpacket(target);
	ASSERT( ! target->isHere());
	shared_ptr<atomic_shared_ptr<Packet> > branchpoint_super(target->branchpoint());
	if( ! branchpoint_super)
		return false; //Supernode has been destroyed.
	target = *branchpoint_super;
	if(target->isBundled())
		return true;
	if( ! target->isHere()) {
		if( ! trySnapshotSuper(*branchpoint_super, target))
			return false;
	}
	ASSERT(target->size());
	if(branchpoint == oldpacket) {
		ASSERT( ! oldpacket->isHere());
		return true; //Up-to-date unbundled packet w/o local packet.
	}
	return false;
}

Node::BundledStatus
Node::bundle(local_shared_ptr<Packet> &target) {
	ASSERT( ! target->isBundled() && target->isHere());
	ASSERT(target->size());
	local_shared_ptr<Packet> prebundled(new Packet(*target));
	//copying all sub-packets from nodes to the new packet.
	prebundled->subpackets().reset(new PacketList(*prebundled->subpackets()));
	shared_ptr<PacketList> &packets(prebundled->subpackets());
	std::vector<local_shared_ptr<Packet> > subpacket_org(target->size());
	for(unsigned int i = 0; i < packets->size(); ++i) {
		shared_ptr<Node> child(prebundled->subnodes()->at(i));
		local_shared_ptr<Packet> &subpacket_new(packets->at(i));
		for(;;) {
			local_shared_ptr<Packet> packetonnode(*child->m_packet);
			if(packetonnode->isHere()) {
				if( ! packetonnode->isBundled()) {
					BundledStatus status = child->bundle(packetonnode);
					if((status == BUNDLE_DISTURBED) &&
						(target == *m_packet))
							continue;
					if(status != BUNDLE_SUCCESS)
						return BUNDLE_DISTURBED;
				}
				subpacket_new = packetonnode;
				ASSERT(packetonnode->isBundled());
			}
			else {
				shared_ptr<atomic_shared_ptr<Packet> > branchpoint(packetonnode->branchpoint());
				if( ! branchpoint)
					return BUNDLE_DISTURBED; //Supernode has been destroyed.
				if(branchpoint != m_packet) {
					//bundled by another node.
					UnbundledStatus status = unbundle(*branchpoint, *child->m_packet,
							packetonnode, NULL, &subpacket_new);
					if((status == UNBUNDLE_SUCCESS) || (status == UNBUNDLE_DISTURBED))
						if(target == *m_packet)
							continue;
					if((status != UNBUNDLE_W_NEW_SUBVALUE) && (status != UNBUNDLE_W_NEW_VALUES))
						return BUNDLE_DISTURBED;
					packetonnode = subpacket_new;
					ASSERT(subpacket_new);
				}
			}
			if( ! subpacket_new) {
//				printf("?");
				ASSERT(target != *m_packet);
				//m_packet has changed, bundled by the other thread.
				return BUNDLE_DISTURBED;
			}
			subpacket_org[i] = packetonnode;
			if(subpacket_new->branchpoint() != m_packet) {
				subpacket_new.reset(new Packet(*subpacket_new));
				subpacket_new->m_branchpoint = m_packet;
			}
			if(subpacket_new->size()) {
				if((subpacket_new->subnodes()->m_superNodeList.lock() != prebundled->subnodes()) ||
					(subpacket_new->subnodes()->m_index != i)) {
					subpacket_new.reset(new Packet(*subpacket_new));
					subpacket_new->subpackets().reset(new PacketList(*subpacket_new->subpackets()));
					subpacket_new->subnodes().reset(new NodeList(*subpacket_new->subnodes()));
					subpacket_new->subnodes()->m_superNodeList = prebundled->subnodes();
					subpacket_new->subnodes()->m_index = i;
				}
			}
			ASSERT(&subpacket_new->node() == child.get());
			break;
		}
		ASSERT(subpacket_new);
		ASSERT(subpacket_new->isBundled());
		if(subpacket_new->size()) {
			ASSERT(subpacket_new->subnodes()->m_superNodeList.lock() == prebundled->subnodes());
			ASSERT(subpacket_new->subnodes()->m_index == i);
		}
	}
	//First checkpoint.
	if( ! m_packet->compareAndSet(target, prebundled)) {
		return BUNDLE_DISTURBED;
	}
	//clearing all packets on sub-nodes if not modified.
	for(unsigned int i = 0; i < prebundled->size(); i++) {
		shared_ptr<Node> child(prebundled->subnodes()->at(i));
		local_shared_ptr<Packet> nullpacket(new NullPacket(m_packet));
		//Second checkpoint, the written bundle is valid or not.
		if( ! child->m_packet->compareAndSet(subpacket_org[i], nullpacket)) {
			return BUNDLE_DISTURBED;
		}
	}
	target.reset(new Packet(*prebundled));
	target->setBundled(true);
	//Finally, tagging as bundled.
	if( ! m_packet->compareAndSet(prebundled, target))
		return BUNDLE_DISTURBED;
	return BUNDLE_SUCCESS;
}

bool
Node::commit(const local_shared_ptr<Packet> &oldpacket, local_shared_ptr<Packet> &newpacket) {
	for(;;) {
		local_shared_ptr<Packet> packet(*m_packet);
		if(packet->isHere()) {
			if(packet != oldpacket)
				return false;
			if(m_packet->compareAndSet(oldpacket, newpacket))
				return true;
			continue;
		}
		shared_ptr<atomic_shared_ptr<Packet> > branchpoint_super(packet->branchpoint());
		if( ! branchpoint_super)
			continue; //Supernode has been destroyed.
		UnbundledStatus status = unbundle(*branchpoint_super, *m_packet, packet, &oldpacket, &newpacket);
		switch(status) {
		case UNBUNDLE_W_NEW_SUBVALUE:
		case UNBUNDLE_W_NEW_VALUES:
			return true;
		case UNBUNDLE_SUBVALUE_HAS_CHANGED:
			return false;
		case UNBUNDLE_SUCCESS:
		case UNBUNDLE_DISTURBED:
		default:
			continue;
		}
	}
}

Node::UnbundledStatus
Node::unbundle(atomic_shared_ptr<Packet> &branchpoint,
	atomic_shared_ptr<Packet> &subbranchpoint, const local_shared_ptr<Packet> &nullpacket,
	const local_shared_ptr<Packet> *oldsubpacket, local_shared_ptr<Packet> *newsubpacket,
	const local_shared_ptr<Packet> *oldsuperpacket, const local_shared_ptr<Packet> *newsuperpacket) {
	ASSERT( ! nullpacket->isHere());
	local_shared_ptr<Packet> packet(branchpoint);
	local_shared_ptr<Packet> copied;
//	printf("u");
	if( ! packet->isHere()) {
		//Unbundle all supernodes.
		if(oldsuperpacket) {
			copied.reset(new Packet(**oldsuperpacket));
			copied->setBundled(false);
		}
		shared_ptr<atomic_shared_ptr<Packet> > branchpoint_super(packet->branchpoint());
		if( ! branchpoint_super)
			return UNBUNDLE_DISTURBED; //Supernode has been destroyed.
		UnbundledStatus ret = unbundle(*branchpoint_super, branchpoint, packet,
			oldsuperpacket ? oldsuperpacket : NULL, &copied);
		if((ret != UNBUNDLE_W_NEW_SUBVALUE) || (ret != UNBUNDLE_W_NEW_VALUES))
			return UNBUNDLE_DISTURBED;
		ASSERT(copied);
	}
	else {
		if( ! packet->size())
			return UNBUNDLE_SUBVALUE_HAS_CHANGED;
		copied.reset(new Packet(*packet));
		copied->setBundled(false);
		if(newsuperpacket)
			if(packet != *oldsuperpacket)
				return UNBUNDLE_DISTURBED;
		//Tagging as unbundled.
		if( ! branchpoint.compareAndSet(packet, copied)) {
			return UNBUNDLE_DISTURBED;
		}
	}

	if( ! copied->size())
		return UNBUNDLE_SUBVALUE_HAS_CHANGED;
	local_shared_ptr<Packet> subpacket;
	NodeList::iterator nit = copied->subnodes()->begin();
	for(PacketList::iterator pit = copied->subpackets()->begin(); pit != copied->subpackets()->end();) {
		if((*nit)->m_packet.get() == &subbranchpoint) {
			subpacket = *pit;
		}
		++pit;
		++nit;
	}
	if( ! subpacket)
		return UNBUNDLE_SUBVALUE_HAS_CHANGED;

	local_shared_ptr<Packet> newsubpacket_copied;
	if(oldsubpacket) {
		newsubpacket_copied = *newsubpacket;
		if(subpacket != *oldsubpacket) {
			return UNBUNDLE_SUBVALUE_HAS_CHANGED;
		}
	}
	else {
		if( ! subpacket)
			return UNBUNDLE_DISTURBED;
		newsubpacket_copied = subpacket;
		if(newsubpacket_copied->size()) {
			newsubpacket_copied.reset(new Packet(*newsubpacket_copied));
//			newsubpacket_copied->setBundled(false);
		}
	}

	if( ! subbranchpoint.compareAndSet(nullpacket, newsubpacket_copied)) {
		if( ! local_shared_ptr<Packet>(subbranchpoint)->isHere())
			return UNBUNDLE_SUBVALUE_HAS_CHANGED;
		return UNBUNDLE_SUCCESS;
	}
	*newsubpacket = newsubpacket_copied;
	if(newsuperpacket) {
		packet = *newsuperpacket;
	}
	else {
		//Erasing out-of-date subpackets on the unbundled superpacket.
		packet.reset(new Packet(*copied));
		packet->subpackets().reset(new PacketList(*packet->subpackets()));
		NodeList::iterator nit = packet->subnodes()->begin();
		for(PacketList::iterator pit = packet->subpackets()->begin(); pit != packet->subpackets()->end();) {
			if(*pit) {
				local_shared_ptr<Packet> subpacket(*(*nit)->m_packet);
				if(subpacket->isHere()) {
					//Touch (*nit)->m_packet once before erasing.
					if(((*nit)->m_packet.get() == &subbranchpoint) ||
						(*nit)->m_packet->compareAndSet(*pit, local_shared_ptr<Packet>(new Packet(**pit)))) {
						pit->reset();
					}
				}
			}
			++pit;
			++nit;
		}
	}
	if(branchpoint.compareAndSet(copied, packet))
		return UNBUNDLE_W_NEW_VALUES;
	else
		return UNBUNDLE_W_NEW_SUBVALUE;
}

