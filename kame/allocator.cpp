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

#include "allocator.h"

#include "support.h"
#include "atomic.h"

template <int SIZE>
inline void *
FixedSizeAllocator<SIZE>::allocate_pooled() {
	for(int cnt = 0; cnt < MEMPOOL_COUNT; ++cnt) {
		int idx = m_idx;
		if( !m_flags[idx]) {
//				if(atomicSwap(1, &m_flags[idx]) == 0) {
			if(atomicCompareAndSet((FUINT)0, (FUINT)1, &m_flags[idx])) {
				readBarrier();
				return &m_mempool[idx * SIZE];
			}
		}
		++idx;
		if(idx == MEMPOOL_COUNT)
			idx = 0;
		m_idx = idx;
	}
	return 0;
}
template <int SIZE>
inline bool
FixedSizeAllocator<SIZE>::deallocate_pooled(void *p) {
	if((p < m_mempool) || (p >= &m_mempool[ALLOC_MEMPOOL_SIZE]))
		return false;
	int idx = static_cast<size_t>((char *)p - m_mempool) / SIZE;
	ASSERT(m_flags[idx] == 1);
	writeBarrier();
	m_flags[idx] = 0;
//		m_idx = idx;
	return true;
}
template <int SIZE>
bool
FixedSizeAllocator<SIZE>::trySetupNewAllocator(int aidx) {
	FixedSizeAllocator *alloc = new FixedSizeAllocator;
	atomicInc( &s_allocators_cnt);
	writeBarrier();
	if(atomicCompareAndSet((FixedSizeAllocator *)0, alloc, &s_allocators[aidx]))
		return true;

	delete alloc;
	atomicDec( &s_allocators_cnt);
	return false;
}
template <int SIZE>
inline void *
FixedSizeAllocator<SIZE>::allocate(size_t size) {
	int acnt = s_allocators_cnt;
	int aidx = s_curr_allocator_idx;
	for(int cnt = 0;; ++cnt) {
		FixedSizeAllocator *alloc = s_allocators[aidx];
		if(alloc) {
			if(void *p = alloc->allocate_pooled()) {
				for(unsigned int i = 0; i < SIZE / sizeof(uint64_t); ++i)
					static_cast<uint64_t *>(p)[i] = 0; //zero clear.
				return p;
			}
			++aidx;
			if((aidx >= acnt) && (cnt < acnt))
				aidx = 0;
		}
		else {
			trySetupNewAllocator(aidx);
			acnt = s_allocators_cnt;
		}
		s_curr_allocator_idx = aidx;
	}
}
template <int SIZE>
inline bool
FixedSizeAllocator<SIZE>::deallocate(void *p) {
	int acnt = s_allocators_cnt;
	int aidx = std::min(s_curr_allocator_idx, acnt - 1);
	for(int cnt = 0; cnt < acnt; ++cnt) {
		FixedSizeAllocator *alloc = s_allocators[aidx];
		if(alloc) {
			if(alloc->deallocate_pooled(p)) {
				s_curr_allocator_idx = aidx;
				return true;
			}
		}
		if(aidx == 0)
			aidx = acnt - 1;
		else
			--aidx;
	}
	return false;
}
template <int SIZE>
void*
FixedSizeAllocator<SIZE>::operator new(size_t size) throw() {
	return malloc(size);
}
template <int SIZE>
void
FixedSizeAllocator<SIZE>::operator delete(void* p) {
	free(p);
}

void operator delete(void* p) throw() {
	if(FixedSizeAllocator<ALLOC_SIZE1>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE2>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE3>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE4>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE5>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE6>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE7>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE8>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE9>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE10>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE11>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE12>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE13>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE14>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE15>::deallocate(p))
		return;
	if(FixedSizeAllocator<ALLOC_SIZE16>::deallocate(p))
		return;
	free(p);
}
template <int>
FixedSizeAllocator<ALLOC_SIZE1> *FixedSizeAllocator<ALLOC_SIZE1>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE1>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE1>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE2> *FixedSizeAllocator<ALLOC_SIZE2>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE2>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE2>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE3> *FixedSizeAllocator<ALLOC_SIZE3>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE3>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE3>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE4> *FixedSizeAllocator<ALLOC_SIZE4>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE4>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE4>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE5> *FixedSizeAllocator<ALLOC_SIZE5>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE5>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE5>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE6> *FixedSizeAllocator<ALLOC_SIZE6>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE6>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE6>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE7> *FixedSizeAllocator<ALLOC_SIZE7>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE7>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE7>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE8> *FixedSizeAllocator<ALLOC_SIZE8>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE8>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE8>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE9> *FixedSizeAllocator<ALLOC_SIZE9>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE9>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE9>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE10> *FixedSizeAllocator<ALLOC_SIZE10>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE10>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE10>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE11> *FixedSizeAllocator<ALLOC_SIZE11>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE11>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE11>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE12> *FixedSizeAllocator<ALLOC_SIZE12>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE12>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE12>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE13> *FixedSizeAllocator<ALLOC_SIZE13>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE13>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE13>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE14> *FixedSizeAllocator<ALLOC_SIZE14>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE14>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE14>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE15> *FixedSizeAllocator<ALLOC_SIZE15>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE15>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE15>::s_allocators_cnt;
template <int>
FixedSizeAllocator<ALLOC_SIZE16> *FixedSizeAllocator<ALLOC_SIZE16>::s_allocators[ALLOC_MAX_ALLOCATORS];
template <int>
int FixedSizeAllocator<ALLOC_SIZE16>::s_curr_allocator_idx;
template <int>
int FixedSizeAllocator<ALLOC_SIZE16>::s_allocators_cnt;

template <int SIZE>
FixedSizeAllocator<SIZE>::FixedSizeAllocator()  : m_mempool(new char[ALLOC_MEMPOOL_SIZE]), m_idx(0) {
	fprintf(stderr, "new allocator for %dB, starting @ %llx, ending @ %llx\n", SIZE,
		(unsigned long long)m_mempool, (unsigned long long)( &m_mempool[ALLOC_MEMPOOL_SIZE]));
	memset(m_flags, 0, MEMPOOL_COUNT);
	C_ASSERT(SIZE % sizeof(uint64_t) == 0);
	ASSERT((uintptr_t)m_mempool % sizeof(uint64_t) == 0);
	memoryBarrier();
}
template <int SIZE>
FixedSizeAllocator<SIZE>::~FixedSizeAllocator() {
	delete [] m_mempool;
}

template class FixedSizeAllocator<ALLOC_SIZE1>;
template class FixedSizeAllocator<ALLOC_SIZE2>;
template class FixedSizeAllocator<ALLOC_SIZE3>;
template class FixedSizeAllocator<ALLOC_SIZE4>;
template class FixedSizeAllocator<ALLOC_SIZE5>;
template class FixedSizeAllocator<ALLOC_SIZE6>;
template class FixedSizeAllocator<ALLOC_SIZE7>;
template class FixedSizeAllocator<ALLOC_SIZE8>;
template class FixedSizeAllocator<ALLOC_SIZE9>;
template class FixedSizeAllocator<ALLOC_SIZE10>;
template class FixedSizeAllocator<ALLOC_SIZE11>;
template class FixedSizeAllocator<ALLOC_SIZE12>;
template class FixedSizeAllocator<ALLOC_SIZE13>;
template class FixedSizeAllocator<ALLOC_SIZE14>;
template class FixedSizeAllocator<ALLOC_SIZE15>;
template class FixedSizeAllocator<ALLOC_SIZE16>;
