/*******************************************************************************
 *
 * Copyright (c) 2016 Seller Tamás. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *******************************************************************************/

#ifndef MOCKFLASHDRIVER_H_
#define MOCKFLASHDRIVER_H_

#include "1test/Mock.h"

#include "ubiquitous/Trace.h"

class MockFlashTrace: public pet::Trace<MockFlashTrace> {};

template<unsigned int bytesPerPage, unsigned int pagesPerBlock, unsigned int nBlocks>
class MockFlashDriver {
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
		MockFlashTrace::info << "erase: " << blockAddress << "\n";
		for(unsigned int i=0; i<blockSize; i++)
			for(unsigned int j=0; j<pageSize; j++)
				blocks[blockAddress][i][j] = 0xff;
	}

	static void read(Address addr, void* data) {
		MockFlashTrace::info << "read: " << addr << "\n";
		MOCK("FlashDriver")::CALL("read").withParam(addr);

		for(unsigned int i=0; i<pageSize; i++)
			((char*)data)[i] = blocks[addr / blockSize][addr % blockSize][i];
	}

	static void write(Address addr, void* data) {
		MockFlashTrace::info << "write: " << addr << "\n";
		MOCK("FlashDriver")::CALL("write").withParam(addr);

		for(unsigned int i=0; i<pageSize; i++)
			blocks[addr / blockSize][addr % blockSize][i] &= ((char*)data)[i];
	}
};

template<unsigned int bytesPerPage, unsigned int pagesPerBlock, unsigned int nBlocks>
typename MockFlashDriver<bytesPerPage, pagesPerBlock, nBlocks>::Block
MockFlashDriver<bytesPerPage, pagesPerBlock, nBlocks>::blocks[nBlocks];

#endif /* MOCKFLASHDRIVER_H_ */
