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

#include "BTreeParametrizedForTesting.h"

#include "PoorHashKey.h"

#include "MockStorage.h"
#include "pet/test/MockAllocator.h"

#include "BTreeCommon.h"

typedef BTree<class Storage, PoorHashKey, PoorHashIndexKey, int, FailableAllocator> TestTree;
class Storage: public MockStorage<5*2*sizeof(int), TestTree> {};

TEST_GROUP(PoorHash) {
	TestTree tree;
	static constexpr unsigned int size = 100;
	static constexpr unsigned int scale = size/5;

	TEST_SETUP() {
		pet::FailureInjector::disable();
		for(unsigned int i=0; i<size; i += 2) {
			pet::GenericError ret = tree.put(10*i, 5*i);
			CHECK(!ret.failed());
			CHECK(ret);
		}

		for(int i=size-1; i>0; i -= 2) {
			pet::GenericError ret = tree.put(10*i, 5*i);
			CHECK(!ret.failed());
			CHECK(ret);
		}

		pet::FailureInjector::enable();
	}

	TEST_TEARDOWN() {
		int i = 0;
		pet::FailureInjector::disable();
		for(unsigned int i=0; i<size; i++) {
			pet::GenericError ret = tree.remove(10*i);
			CHECK(!ret.failed());
		}
		pet::FailureInjector::enable();

		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(PoorHash, AddOne) {
	pet::GenericError ret = tree.put(123, -1);
	CHECK(!ret.failed());
	CHECK(ret);
};

TEST(PoorHash, DontInsert) {
	for(unsigned int i=0; i<size/scale; i++) {
		pet::GenericError ret = tree.insert(scale*10*i, -1);
		CHECK(!ret.failed());
		CHECK(!ret);
	}
};

TEST(PoorHash, DontUpdate) {
	pet::GenericError ret = tree.update(1, -1);
	CHECK(!ret.failed());
	CHECK(!ret);
};

TEST(PoorHash, GetOne) {
	PoorHashKey key(100);
	int value;
	pet::GenericError ret = tree.get(key, value);
	CHECK(!ret.failed());
	CHECK(ret);
	CHECK(key == 100);
	CHECK(value == 50);
};

TEST(PoorHash, DontGet) {
	PoorHashKey key(123);
	int value;
	pet::GenericError ret = tree.get(key, value);
	CHECK(!ret.failed());
	CHECK(!ret);
	CHECK(key == 123);
};

TEST(PoorHash, RemoveOne) {
	pet::GenericError ret = tree.remove(100);
	CHECK(!ret.failed());
	CHECK(ret);
};

TEST(PoorHash, DontRemove) {
	pet::GenericError ret = tree.remove(101);
	CHECK(!ret.failed());
	CHECK(!ret);
};

