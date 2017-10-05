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

#include "1test/Test.h"
#include "pet/test/MockAllocator.h"
#include "MockStorage.h"
#include "1test/FailureInjector.h"

#include "blob/BlobTree.h"

class TestTree;

namespace {
typedef MockStorage<3*sizeof(void*), TestTree, false, false> Storage;
struct TestTree: public BlobTree<Storage, FailableAllocator, 1> {
	using Storage::Address;
	inline TestTree(Address fileRoot, unsigned int size): BlobTree(fileRoot, size) {}

	Address findNth(unsigned int n=1) {
		pet::FailureInjector::disable();
		Storage::ReadWriteSession session(this);
		Address retAddr;

		pet::GenericError ret = traverse(session, [&](typename Storage::Address addr, unsigned int level, const Traversor&) -> typename Storage::Address {
			if(--n)
				return addr;

			retAddr = addr;
			return Storage::InvalidAddress;
		});

		ret.failed(); // Nothing to do about it

		this->closeReadWriteSession(session);

		pet::FailureInjector::enable();
		return retAddr;
	}
};

struct TestData {
	TestTree tree;
	inline TestData(): tree(Storage::InvalidAddress, 0) {}

	inline void addNPages(unsigned int n) {
		pet::FailureInjector::disable();
		for(unsigned int i = 0; i < n; i++) {
			pet::FailPointer<void> ret = tree.empty();
			CHECK(!ret.failed());
			unsigned char* buffer = ret;

			for(unsigned int j=0; j<Storage::pageSize; j++)
				buffer[j] = j;

			pet::GenericError result = tree.update(i, Storage::pageSize*(i+1), buffer);
			CHECK(!result.failed());
		}
		pet::FailureInjector::enable();
	}

	inline void append() {
	pet::FailPointer<void> ret = tree.empty();
		CHECK(!ret.failed());

		if(ret.failed())
			return;

		unsigned char* buffer = ret;

		for(unsigned int i=Storage::pageSize*1/3; i<Storage::pageSize*3/4; i++)
			buffer[i] = i;

		pet::GenericError result = tree.update(tree.getSize()/Storage::pageSize, tree.getSize()+Storage::pageSize, buffer);
		CHECK(!result.failed());
	}

	inline void update() {
		pet::FailPointer<void> ret = tree.read(1);
		CHECK(!ret.failed());

		if(ret.failed())
			return;

		unsigned char* buffer = ret;

		for(unsigned int i=Storage::pageSize*1/3; i<Storage::pageSize*2/3; i++)
			buffer[i] = -i;

		pet::GenericError result = tree.update(1, tree.getSize(), buffer);
		CHECK(!result.failed());
	}

	void teardown() {
		pet::FailureInjector::disable();
		CHECK_ALWAYS(FailableAllocator::allFreed());
		CHECK(!tree.dispose().failed());
		CHECK_ALWAYS(Storage::isClean());
		pet::FailureInjector::enable();
	}
};
}

TEST_GROUP(Empty) {
	TestData test;

	TEST_TEARDOWN() {
		test.teardown();
	}
};

TEST(Empty, InvalidRead) {
	pet::FailPointer<void> ret = test.tree.read(0);
	CHECK(ret.failed());
}

TEST(Empty, VeryInvalidRead) {
	pet::FailPointer<void> ret = test.tree.read(1);
	CHECK(ret.failed());
}

TEST(Empty, GetSize) {
	CHECK(test.tree.getSize() == 0);
}

TEST(Empty, AppendOne) {
	pet::FailPointer<void> ret = test.tree.empty();
	CHECK(!ret.failed());

	if(ret.failed())
		return;

	void *buffer = ret;

	pet::GenericError result = test.tree.update(0, 1, buffer);
	CHECK(!result.failed());
}

TEST_GROUP(SinglePage) {
	TestData test;

	TEST_SETUP() {
		test.addNPages(1);
	}

	TEST_TEARDOWN() {
		test.teardown();
	}
};

TEST(SinglePage, Read) {
	pet::FailPointer<void> ret = test.tree.read(0);
	CHECK(!ret.failed());
	unsigned char* buffer = ret;

	if(ret.failed())
		return;

	for(unsigned int i=0; i<Storage::pageSize; i++)
		CHECK(buffer[i] == i);

	test.tree.release(buffer);
}

TEST(SinglePage, GetSize) {
	CHECK(test.tree.getSize() == Storage::pageSize);
}

TEST(SinglePage, Update) {
	pet::FailPointer<void> ret = test.tree.read(0);
	CHECK(!ret.failed());
	unsigned char* buffer = ret;

	if(ret.failed())
		return;

	for(unsigned int i=Storage::pageSize*1/3; i<Storage::pageSize*2/3; i++)
		buffer[i] = -i;

	pet::GenericError result = test.tree.update(0, test.tree.getSize(), buffer);
	CHECK(!result.failed());
}

TEST(SinglePage, Append) {
	test.append();
}

TEST(SinglePage, Rewrite) {
	Storage::Address addr = test.tree.findNth();
	CHECK(!test.tree.relocate(addr).failed());
}

TEST_GROUP(TwoPages) {
	TestData test;

	TEST_SETUP() {
		test.addNPages(2);
	}

	TEST_TEARDOWN() {
		test.teardown();
	}
};

TEST(TwoPages, Update) {
	test.update();
}

TEST(TwoPages, Append) {
	test.append();
}

TEST(TwoPages, Rewrite) {
	Storage::Address addr = test.tree.findNth();
	CHECK(!test.tree.relocate(addr).failed());
}


TEST_GROUP(ThreePages) {
	TestData test;

	TEST_SETUP() {
		test.addNPages(3);
	}

	TEST_TEARDOWN() {
		test.teardown();
	}
};

TEST(ThreePages, Update) {
	test.update();
}

TEST(ThreePages, Append) {
	test.append();
}

TEST(ThreePages, Rewrite) {
	Storage::Address addr = test.tree.findNth();
	CHECK(!test.tree.relocate(addr).failed());
}

TEST_GROUP(NinePages) {
	TestData test;

	TEST_SETUP() {
		test.addNPages(9);
	}

	TEST_TEARDOWN() {
		test.teardown();
	}
};

TEST(NinePages, Update) {
	test.update();
}

TEST(NinePages, Append) {
	test.append();
}

TEST(NinePages, Rewrite1stData) {
	Storage::Address addr = test.tree.findNth();
	CHECK(!test.tree.relocate(addr).failed());
}

TEST(NinePages, Rewrite2ndData) {
	Storage::Address addr = test.tree.findNth(2);
	CHECK(!test.tree.relocate(addr).failed());
}

TEST(NinePages, Rewrite3rdData) {
	Storage::Address addr = test.tree.findNth(3);
	CHECK(!test.tree.relocate(addr).failed());
}

TEST(NinePages, Rewrite1stIndex) {
	Storage::Address addr = test.tree.findNth(4);
	CHECK(!test.tree.relocate(addr).failed());
}

TEST(NinePages, Rewrite2ndIndex) {
	Storage::Address addr = test.tree.findNth(8);
	CHECK(!test.tree.relocate(addr).failed());
}

TEST(NinePages, Rewrite3rdIndex) {
	Storage::Address addr = test.tree.findNth(12);
	CHECK(!test.tree.relocate(addr).failed());
}

TEST(NinePages, RewriteRoot) {
	Storage::Address addr = test.tree.findNth(13);
	CHECK(!test.tree.relocate(addr).failed());
}

TEST_GROUP(ManyPages) {
	TestData test;

	TEST_SETUP() {
		test.addNPages(27);
	}

	TEST_TEARDOWN() {
		test.teardown();
	}
};

TEST(ManyPages, Update) {
	test.update();
}

TEST(ManyPages, Append) {
	test.append();
}

TEST(ManyPages, Purge) {
	CHECK(!test.tree.dispose().failed());
}
