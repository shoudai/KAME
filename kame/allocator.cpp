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

#include <string.h>

template <typename T>
unsigned int count_bits(T x);

//! Bit count / population count for 32bit.
//! Referred to H. S. Warren, Jr., "Beautiful Code", O'Reilly.
template <>
inline unsigned int count_bits(uint32_t x) {
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0f0f0f0fu;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0xffu;
}

template <>
inline unsigned int count_bits(uint64_t x) {
    x = x - ((x >> 1) & 0x5555555555555555uLL);
    x = (x & 0x3333333333333333uLL) + ((x >> 2) & 0x3333333333333333uLL);
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fuLL;
    x = x + (x >> 8);
    x = x + (x >> 16);
    x = x + (x >> 32);
    return x & 0xffu;
}

//! Folds "OR" operations. O(log X).
//! Expecting inline expansions of codes.
template <unsigned int X, unsigned int SHIFTS, typename T>
inline T fold_bits(T x) {
//	printf("%d, %llx\n", SHIFTS, x);
	if(X <  2 * SHIFTS)
		return x;
	x = (x >> SHIFTS) | x;
	if(X & SHIFTS)
		x = (x >> SHIFTS) | x;
	return fold_bits<X, (2 * SHIFTS < sizeof(T) * 8) ? 2 * SHIFTS : 1>(x);
}

//! \return one bit at the first zero from LSB in \a x.
template <typename T>
inline T find_first_zero(T x) {
	return ~((x) | (~x - 1u));
}

//! \return one bit at the first one from LSB in \a x.
template <typename T>
inline T find_first_one(T x) {
	return x & (-x);
}

//! Finds training zeros from LSB in \a x using O(log n) algorithm.
//! \tparam X number of zeros to be looked for.
//! \return one bit at the LSB of the training zeros if enough zeros are found.
template <int X, typename T>
T find_training_zeros(T x) {
	if(X == sizeof(T) * 8)
		return !x ? 1u : 0; //a trivial case.
	x = fold_bits<X, 1>(x);
	x = find_first_zero(x); //picking the first zero from LSB.
	if(x > (T)1u << (sizeof(T) * 8 - X)) return 0; //checking if T has enough space in MSBs.
	return x;
}

PooledAllocator::PooledAllocator()  : m_idx(0) {
	memset(m_flags, 0, FLAGS_COUNT * sizeof(FUINT));
	memset(m_sizes, 0, FLAGS_COUNT * sizeof(FUINT));
	ASSERT((uintptr_t)m_mempool % ALLOC_ALIGNMENT == 0);
//	memoryBarrier();
}
PooledAllocator::~PooledAllocator() {
	for(int idx = 0; idx < FLAGS_COUNT; ++idx) {
		while(m_flags[idx]) {
			int sidx = count_bits(find_first_one(m_flags[idx]) - 1);
			int size = count_bits(find_first_zero(m_sizes[idx] >> sidx) - 1) + 1;
			fprintf(stderr, "Leak found for %dB @ %llx.\n", size,
				(unsigned long long)(uintptr_t) &m_mempool[(idx * sizeof(FUINT) * 8 + sidx) * ALLOC_ALIGNMENT]);
		}
	}
}

template <unsigned int SIZE>
inline void *
PooledAllocator::allocate_pooled() {
	for(int cnt = 0; cnt < FLAGS_COUNT; ++cnt) {
		int idx = m_idx;
		FUINT oldv = m_flags[idx];
		if(FUINT cand = find_training_zeros<SIZE / ALLOC_ALIGNMENT>(oldv)) {
			FUINT ones = cand *
				(2u * (((FUINT)1u << (SIZE / ALLOC_ALIGNMENT - 1u)) - 1u) + 1u); //N ones, no to cause compilation warning.
//			ASSERT(count_bits(ones) == SIZE / ALLOC_ALIGNMENT);
			FUINT newv = oldv | ones; //filling with SIZE ones.
//			ASSERT( !(ones & oldv));
			if(atomicCompareAndSet(oldv, newv, &m_flags[idx])) {
				for(;;) {
					FUINT sizes_old = m_sizes[idx];
					FUINT sizes_new = (sizes_old | ones) & ~(cand << (SIZE / ALLOC_ALIGNMENT - 1u));
					if(atomicCompareAndSet(sizes_old, sizes_new, &m_sizes[idx])) {
						break;
					}
				}
				int sidx = count_bits(cand - 1);
				return &m_mempool[(idx * sizeof(FUINT) * 8 + sidx) * ALLOC_ALIGNMENT];
			}
			readBarrier();
		}
		++idx;
		if(idx == FLAGS_COUNT)
			idx = 0;
		m_idx = idx;
	}
	return 0;
}
inline bool
PooledAllocator::deallocate_pooled(void *p) {
	int midx = static_cast<size_t>((char *)p - m_mempool) / ALLOC_ALIGNMENT;
	int idx = midx / (sizeof(FUINT) * 8);
	unsigned int sidx = midx % (sizeof(FUINT) * 8);
	FUINT subones = ((FUINT)1u << sidx) - 1u;
	FUINT ones = find_first_zero(m_sizes[idx] |  subones);
	ones = (ones | (ones - 1)) & ~subones;
	for(;;) {
		FUINT oldv = m_flags[idx];
		FUINT newv = oldv & ~ones;
//		fprintf(stderr, "d: %llx, %d, %x, %x, %x\n", (unsigned long long)(uintptr_t)p, idx, oldv, newv, ones);
		ASSERT(( ~oldv & ones) == 0); //checking for double free.
		if(atomicCompareAndSet(oldv, newv, &m_flags[idx]))
			break;
	}
	m_idx = idx; //writing a hint for a next allocation.
	return true;
}
bool
PooledAllocator::trySetupNewAllocator(int aidx) {
	PooledAllocator *alloc = new PooledAllocator;
	if(atomicCompareAndSet((PooledAllocator *)0, alloc, &s_allocators[aidx])) {
		writeBarrier(); //for alloc.
		atomicInc( &s_allocators_cnt);
//		writeBarrier();
		fprintf(stderr, "New memory pool, starting @ %llx w/ len. of %llxB.\n",
			(unsigned long long)(uintptr_t)alloc->m_mempool,
			(unsigned long long)(uintptr_t)ALLOC_MEMPOOL_SIZE);
		return true;
	}
	delete alloc;
	return false;
}
template <unsigned int SIZE>
void *
PooledAllocator::allocate() {
	int acnt = s_allocators_cnt;
	int aidx = std::min(s_curr_allocator_idx, acnt - 1);
	readBarrier(); //for alloc.
	for(int cnt = 0;; ++cnt) {
		if(cnt == acnt) {
			trySetupNewAllocator(acnt);
			acnt = s_allocators_cnt;
			readBarrier(); //for alloc.
			aidx = acnt - 1;
		}
		PooledAllocator *alloc = s_allocators[aidx];
		if(void *p = alloc->allocate_pooled<SIZE>()) {
//			fprintf(stderr, "a: %llx\n", (unsigned long long)(uintptr_t)p);
			readBarrier(); //for the pooled memory.
			for(unsigned int i = 0; i < SIZE / sizeof(uint64_t); ++i)
				static_cast<uint64_t *>(p)[i] = 0; //zero clear.
			return p;
		}
		++aidx;
		if(aidx >= acnt)
			aidx = 0;
		s_curr_allocator_idx = aidx;
	}
}
inline bool
PooledAllocator::deallocate(void *p) {
	int acnt = s_allocators_cnt;
	int aidx = std::min(s_curr_allocator_idx, acnt - 1);
	for(int cnt = 0; cnt < acnt; ++cnt) {
		PooledAllocator *alloc = s_allocators[aidx];
		if((p >= alloc->m_mempool) && (p < &alloc->m_mempool[ALLOC_MEMPOOL_SIZE])) {
			if(alloc->deallocate_pooled(p)) {
	//				s_curr_allocator_idx = aidx;
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
void
PooledAllocator::release_pools() {
	int acnt = s_allocators_cnt;
	for(int aidx = 0; aidx < acnt; ++aidx) {
		PooledAllocator *alloc = s_allocators[aidx];
		delete alloc;
	}
}
void*
PooledAllocator::operator new(size_t size) throw() {
	return malloc(size);
}
void
PooledAllocator::operator delete(void* p) {
	free(p);
}

void operator delete(void* p) throw() {
	memoryBarrier(); //for the pooled memory
	if(PooledAllocator::deallocate(p))
		return;
	free(p);
}

void release_pools() {
	PooledAllocator::release_pools();
}

PooledAllocator *PooledAllocator::s_allocators[ALLOC_MAX_ALLOCATORS];
int PooledAllocator::s_curr_allocator_idx;
int PooledAllocator::s_allocators_cnt;

template void *PooledAllocator::allocate<ALLOC_SIZE1>();
template void *PooledAllocator::allocate<ALLOC_SIZE2>();
template void *PooledAllocator::allocate<ALLOC_SIZE3>();
template void *PooledAllocator::allocate<ALLOC_SIZE4>();
template void *PooledAllocator::allocate<ALLOC_SIZE5>();
template void *PooledAllocator::allocate<ALLOC_SIZE6>();
template void *PooledAllocator::allocate<ALLOC_SIZE7>();
template void *PooledAllocator::allocate<ALLOC_SIZE8>();
template void *PooledAllocator::allocate<ALLOC_SIZE9>();
template void *PooledAllocator::allocate<ALLOC_SIZE10>();
template void *PooledAllocator::allocate<ALLOC_SIZE11>();
template void *PooledAllocator::allocate<ALLOC_SIZE12>();
template void *PooledAllocator::allocate<ALLOC_SIZE13>();
template void *PooledAllocator::allocate<ALLOC_SIZE14>();
template void *PooledAllocator::allocate<ALLOC_SIZE15>();
template void *PooledAllocator::allocate<ALLOC_SIZE16>();
template void *PooledAllocator::allocate<ALLOC_SIZE17>();
template void *PooledAllocator::allocate<ALLOC_SIZE18>();
template void *PooledAllocator::allocate<ALLOC_SIZE19>();
template void *PooledAllocator::allocate<ALLOC_SIZE20>();
template void *PooledAllocator::allocate<ALLOC_SIZE21>();

//struct _PoolReleaser {
//	~_PoolReleaser() {
//		release_pools();
//	}
//} _pool_releaser;
