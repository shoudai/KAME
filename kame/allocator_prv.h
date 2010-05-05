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

#ifndef ALLOCATOR_PRV_H_
#define ALLOCATOR_PRV_H_

#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <limits>

#define ALLOC_CHUNK_SIZE (1024 * 256) //256KiB
#if defined __LP64__ || defined __LLP64__
	#define ALLOC_MAX_CHUNKS (1024 * 32) //16GiB max.
	#define ALLOC_MMAP_RESERVE_SIZE (1024 * 1024 * 64) //64MiB
#else
	#define ALLOC_MAX_CHUNKS (1024 * 8) //2GiB max.
	#define ALLOC_MMAP_RESERVE_SIZE (1024 * 1024 * 16) //16MiB
#endif
#define ALLOC_ALIGNMENT (sizeof(double)) //i.e. 8B

class PoolAllocatorBase {
public:
	virtual ~PoolAllocatorBase() {}

	static inline bool deallocate(void *p);
	static void release_chunks();
protected:
	virtual bool deallocate_pooled(void *p) = 0;
	void* operator new(size_t size) throw();
	void operator delete(void* p);

	static bool allocate_chunk(PoolAllocatorBase *p);
	static void deallocate_chunk(int cidx);

	//! A chunk, memory block.
	char *m_mempool;

private:
	enum {NUM_ALLOCATORS_IN_SPACE = ALLOC_MMAP_RESERVE_SIZE / ALLOC_CHUNK_SIZE,
		MMAP_SPACES_COUNT = ALLOC_MAX_CHUNKS / NUM_ALLOCATORS_IN_SPACE};
	//! Swap spaces given by anonymous mmap().
	static char *s_mmapped_spaces[MMAP_SPACES_COUNT];
	static PoolAllocatorBase *s_chunks[ALLOC_MAX_CHUNKS];
};

//! \brief Memory blocks in a unit of double-quad word
//! can be allocated from fixed-size or variable-size memory pools.
//! \tparam FS determines fixed-size or variable-size.
//! \sa allocator_test.cpp.
template <unsigned int ALIGN, bool FS = false, bool DUMMY = true>
class PoolAllocator : public PoolAllocatorBase {
public:
	virtual ~PoolAllocator() { }
	template <unsigned int SIZE>
	static void *allocate();
	static void release_pools();
	void report_leaks();

	typedef uintptr_t FUINT;
protected:
	PoolAllocator();
	template <unsigned int SIZE>
	inline void *allocate_pooled(int aidx);
	virtual bool deallocate_pooled(void *p);
	static bool trySetupNewAllocator(int aidx);
	static bool releaseAllocator(PoolAllocator *alloc);
	enum {FLAGS_COUNT = ALLOC_CHUNK_SIZE / ALIGN / sizeof(FUINT) / 8,
		ALLOC_CHUNKS_COUNT = ALLOC_MAX_CHUNKS / 4};

	//! A hint for searching in a chunk.
	int m_idx;
	//! Every bit indicates occupancy in m_mempool.
	FUINT m_flags[FLAGS_COUNT];
	int m_idx_of_type;

	//! Pointers to PooledAllocator. The LSB bit is set when allocation/releasing/creation is in progress.
	static uintptr_t s_chunks_of_type[ALLOC_CHUNKS_COUNT];
	//! # of flags that having non-zero values.
	static int s_flags_inc_cnt[ALLOC_CHUNKS_COUNT];
	static int s_curr_chunk_idx;
	static int s_chunks_of_type_ubound;
};

//! Partially specialized class for variable-size allocators.
template <unsigned int ALIGN, bool DUMMY>
class PoolAllocator<ALIGN, false, DUMMY> : public PoolAllocator<ALIGN, true, false> {
public:
	void report_leaks();
	typedef typename PoolAllocator<ALIGN, true, false>::FUINT FUINT;
protected:
	enum {FLAGS_COUNT = PoolAllocator<ALIGN, true, false>::FLAGS_COUNT};
	template <unsigned int SIZE>
	inline void *allocate_pooled(int aidx);
	virtual bool deallocate_pooled(void *p);
private:
	template <unsigned int, bool, bool> friend class PoolAllocator;
	//! Cleared bit at the MSB indicates the end of the allocated area. \sa m_flags.
	FUINT m_sizes[FLAGS_COUNT];
};

#define ALLOC_ALIGN1 (ALLOC_ALIGNMENT * 2)
#if defined __LP64__ || defined __LLP64__
	#define ALLOC_ALIGN2 (ALLOC_ALIGNMENT * 16)
	#define ALLOC_ALIGN(size) (((size) % ALLOC_ALIGN2 != 0) || ((size) == ALLOC_ALIGN1 * 64) ? ALLOC_ALIGN1 : ALLOC_ALIGN2)
//	#define ALLOC_ALIGN(size) (((size) <= ALLOC_ALIGN1 * 64) ? ALLOC_ALIGN1 : ALLOC_ALIGN2)
#else
	#define ALLOC_ALIGN2 (ALLOC_ALIGNMENT * 8)
	#define ALLOC_ALIGN3 (ALLOC_ALIGNMENT * 32)
	#define ALLOC_ALIGN(size) (((size) % ALLOC_ALIGN2 != 0) || ((size) == ALLOC_ALIGN1 * 32) ? ALLOC_ALIGN1 :\
		(((size) % ALLOC_ALIGN3 != 0) || ((size) == ALLOC_ALIGN2 * 32) ? ALLOC_ALIGN2 : ALLOC_ALIGN3))
//	#define ALLOC_ALIGN(size) (((size) <= ALLOC_ALIGN1 * 32) ? ALLOC_ALIGN1 :
//		(((size) <= ALLOC_ALIGN2 * 32) ? ALLOC_ALIGN2 : ALLOC_ALIGN3))
#endif

#define ALLOC_SIZE1 (ALLOC_ALIGNMENT * 1)
#define ALLOC_SIZE2 (ALLOC_ALIGNMENT * 2)
#define ALLOC_SIZE3 (ALLOC_ALIGNMENT * 3)
#define ALLOC_SIZE4 (ALLOC_ALIGNMENT * 4)
#define ALLOC_SIZE5 (ALLOC_ALIGNMENT * 5)
#define ALLOC_SIZE6 (ALLOC_ALIGNMENT * 6)
#define ALLOC_SIZE7 (ALLOC_ALIGNMENT * 7)
#define ALLOC_SIZE8 (ALLOC_ALIGNMENT * 8)
#define ALLOC_SIZE9 (ALLOC_ALIGNMENT * 9)
#define ALLOC_SIZE10 (ALLOC_ALIGNMENT * 10)
#define ALLOC_SIZE11 (ALLOC_ALIGNMENT * 11)
#define ALLOC_SIZE12 (ALLOC_ALIGNMENT * 12)
#define ALLOC_SIZE13 (ALLOC_ALIGNMENT * 13)
#define ALLOC_SIZE14 (ALLOC_ALIGNMENT * 14)
#define ALLOC_SIZE15 (ALLOC_ALIGNMENT * 15)
#define ALLOC_SIZE16 (ALLOC_ALIGNMENT * 16)

void* allocate_large_size_or_malloc(size_t size) throw();

#define ALLOCATE_9_16X(X, size) {\
	if(size <= ALLOC_SIZE16 * X) {\
		if(size <= ALLOC_SIZE9 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE9 * X)>::allocate<ALLOC_SIZE9 * X>();\
		if(size <= ALLOC_SIZE10 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE10 * X)>::allocate<ALLOC_SIZE10 * X>();\
		if(size <= ALLOC_SIZE11 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE11 * X)>::allocate<ALLOC_SIZE11 * X>();\
		if(size <= ALLOC_SIZE12 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE12 * X)>::allocate<ALLOC_SIZE12 * X>();\
		if(size <= ALLOC_SIZE13 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE13 * X)>::allocate<ALLOC_SIZE13 * X>();\
		if(size <= ALLOC_SIZE14 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE14 * X)>::allocate<ALLOC_SIZE14 * X>();\
		if(size <= ALLOC_SIZE15 * X)\
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE15 * X)>::allocate<ALLOC_SIZE15 * X>();\
		return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE16 * X)>::allocate<ALLOC_SIZE16 * X>();\
	}\
}

inline void* new_redirected(std::size_t size) throw(std::bad_alloc) {
	//expecting a compile-time optimization because size is usually fixed to the object size.
	if(size <= ALLOC_SIZE1)
		return PoolAllocator<ALLOC_SIZE1, true>::allocate<ALLOC_SIZE1>();
	if(size <= ALLOC_SIZE2)
		return PoolAllocator<ALLOC_SIZE2, true>::allocate<ALLOC_SIZE2>();
	if(size <= ALLOC_SIZE3)
		return PoolAllocator<ALLOC_SIZE3, true>::allocate<ALLOC_SIZE3>();
	if(size <= ALLOC_SIZE4)
		return PoolAllocator<ALLOC_SIZE4, true>::allocate<ALLOC_SIZE4>();
	if(size <= ALLOC_SIZE8) {
		if(size <= ALLOC_SIZE5)
			return PoolAllocator<ALLOC_SIZE5, true>::allocate<ALLOC_SIZE5>();
		if(size <= ALLOC_SIZE6)
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE6)>::allocate<ALLOC_SIZE6>();
		if(size <= ALLOC_SIZE7)
			return PoolAllocator<ALLOC_SIZE7, true>::allocate<ALLOC_SIZE7>();
		return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE8)>::allocate<ALLOC_SIZE8>();
	}
	if(size <= ALLOC_SIZE16) {
		if(size <= ALLOC_SIZE9)
			return PoolAllocator<ALLOC_SIZE9, true>::allocate<ALLOC_SIZE9>();
		if(size <= ALLOC_SIZE10)
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE10)>::allocate<ALLOC_SIZE10>();
		if(size <= ALLOC_SIZE11)
			return PoolAllocator<ALLOC_SIZE11, true>::allocate<ALLOC_SIZE11>();
		if(size <= ALLOC_SIZE12)
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE12)>::allocate<ALLOC_SIZE12>();
		if(size <= ALLOC_SIZE13)
			return PoolAllocator<ALLOC_SIZE13, true>::allocate<ALLOC_SIZE13>();
		if(size <= ALLOC_SIZE14)
			return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE14)>::allocate<ALLOC_SIZE14>();
		if(size <= ALLOC_SIZE15)
			return PoolAllocator<ALLOC_SIZE15, true>::allocate<ALLOC_SIZE15>();
		return PoolAllocator<ALLOC_ALIGN(ALLOC_SIZE16)>::allocate<ALLOC_SIZE16>();
	}
	ALLOCATE_9_16X(2, size);
	return allocate_large_size_or_malloc(size);
}

//void* operator new(std::size_t size) throw(std::bad_alloc);
//void* operator new(std::size_t size, const std::nothrow_t&) throw();
//void* operator new[](std::size_t size) throw(std::bad_alloc);
//void* operator new[](std::size_t size, const std::nothrow_t&) throw();
//
//void operator delete(void* p) throw();
//void operator delete(void* p, const std::nothrow_t&) throw();
//void operator delete[](void* p) throw();
//void operator delete[](void* p, const std::nothrow_t&) throw();

void deallocate_pooled_or_free(void* p) throw();

void release_pools();

#endif /* ALLOCATOR_PRV_H_ */
