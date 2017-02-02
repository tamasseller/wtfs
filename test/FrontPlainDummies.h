/*
 * PlainDummies.h
 *
 *  Created on: 2016.12.17.
 *      Author: tooma
 */

#ifndef PLAINDUMMIES_H_
#define PLAINDUMMIES_H_

#include <malloc.h>

struct PlainDummyAllocator {
	static void* alloc(unsigned int s) {
		return malloc(s);
	}

	static void free(void* p) {
		return ::free(p);
	}
};

template<unsigned int bytesPerPage, unsigned int pagesPerBlock, unsigned int nBlocks>
class PlainDummyFlashDriver {
	typedef char Page[bytesPerPage];
	typedef Page Block[pagesPerBlock];

	static Block blocks[nBlocks];
public:
	typedef unsigned int Address;
	static constexpr unsigned int InvalidAddress=-1u;
	static constexpr unsigned int pageSize = bytesPerPage;
	static constexpr unsigned int blockSize = pagesPerBlock;
	static constexpr unsigned int deviceSize = nBlocks;

	static void ensureErased(unsigned int blockAddress) {
		for(unsigned int i=0; i<blockSize; i++)
			for(unsigned int j=0; j<pageSize; j++)
				blocks[blockAddress][i][j] = 0xff;
	}

	static void read(Address addr, void* data) {
		for(unsigned int i=0; i<pageSize; i++)
			((char*)data)[i] = blocks[addr / blockSize][addr % blockSize][i];
	}

	static void write(Address addr, void* data) {
		for(unsigned int i=0; i<pageSize; i++)
			blocks[addr / blockSize][addr % blockSize][i] &= ((char*)data)[i];
	}
};

template<unsigned int bytesPerPage, unsigned int pagesPerBlock, unsigned int nBlocks>
typename PlainDummyFlashDriver<bytesPerPage, pagesPerBlock, nBlocks>::Block
PlainDummyFlashDriver<bytesPerPage, pagesPerBlock, nBlocks>::blocks[nBlocks];

#endif /* PLAINDUMMIES_H_ */
