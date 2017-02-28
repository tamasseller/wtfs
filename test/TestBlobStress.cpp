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

#include "MockStorage.h"
#include "pet/test/MockAllocator.h"
#include "FailureInjectorPlugin.h"

#include "blob/BlobTree.h"

#include <iostream>

namespace {

class TestTree;

typedef MockStorage<3*sizeof(void*), TestTree, false, false> Storage;
struct TestTree: public BlobTree<Storage, FailableAllocator, 1> {
	using Storage::Address;
	inline TestTree(Address fileRoot, unsigned int size): BlobTree(fileRoot, size) {}
};

template<unsigned int nPages>
struct TestData {
	TestTree tree;
	inline TestData(): tree(Storage::InvalidAddress, 0) {}

	inline void populate() {
		for(int i = 0; i < nPages; i++) {
			pet::FailPointer<void> ret = tree.empty();
			CHECK(!ret.failed());

			if(ret.failed())
				continue;

			unsigned char* buffer = ret;

			for(unsigned int j=0; j<Storage::pageSize; j++)
				buffer[j] = j;

			pet::GenericError result = tree.update(i, Storage::pageSize*(i+1), buffer);
			CHECK(!result.failed());
		}
	}

	inline void modify(unsigned int idx) {
		//std::cout << std::endl << "???? " << idx << std::endl << std::endl;
		pet::FailPointer<void> ret = tree.read(idx);
		CHECK(!ret.failed());

		if(ret.failed())
			return;

		unsigned char* buffer = ret;

		for(unsigned int i=0; i<Storage::pageSize; i++)
			buffer[i] = -i;

		//std::cout << std::endl << "!!!! " << idx << " " << (void*)buffer << std::endl << std::endl;

		pet::GenericError result = tree.update(idx, tree.getSize(), buffer);
		CHECK(!result.failed());
	}
};

}

TEST_GROUP(BlobTreeStress) {
	static constexpr unsigned int nPages = 11;
	static constexpr unsigned int prime2 = 7;
	TestData<nPages> test; // 4 levels

	TEST_TEARDOWN() {
		DISABLE_FAILURE_INJECTION_TEMPORARILY();
		CHECK_ALWAYS(FailableAllocator::allFreed());
		bool canBeDirty = test.tree.dispose().failed();
		bool clean = Storage::isClean();
		CHECK_ALWAYS(canBeDirty || clean);
		ENABLE_FAILURE_INJECTION_TEMPORARILY();
	}
};

TEST(BlobTreeStress, Populate) {
	test.populate();
}

TEST(BlobTreeStress, Modify) {
	DISABLE_FAILURE_INJECTION_TEMPORARILY();
	test.populate();
	ENABLE_FAILURE_INJECTION_TEMPORARILY();

	for(unsigned int i = 1; i; i = (i + prime2) % nPages) {
		test.modify(i);
	}
}
