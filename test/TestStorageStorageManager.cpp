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

#include "MockFlashDriver.h"

#include "storage/StorageManager.h"

#include "1test/Test.h"
#include "1test/Mock.h"

namespace {
	template<unsigned int pageSize, unsigned int blockSize, unsigned int deviceSize, unsigned int maxMeta, unsigned int maxFile>
	struct ParametricTestData: public StorageManager<MockFlashDriver<pageSize, blockSize, deviceSize>, maxMeta, maxFile>{
		typedef MockFlashDriver<pageSize, blockSize, deviceSize> FlashDriver;
		typedef StorageManager<FlashDriver, maxMeta, maxFile> Super;

		static constexpr unsigned int nFileLevels = maxFile;
		static constexpr unsigned int nMetaLevels = maxMeta;
		static constexpr unsigned int nLevels = Super::maxLevels;
		static constexpr unsigned int nBlocks = deviceSize;
		static constexpr unsigned int nPages = blockSize;

		enum BlockState {
			Full, Partial
		};

		BlockState getState(unsigned int blockAddr) {
			return (this->usageCounters[blockAddr] == FlashDriver::blockSize) ? Full : Partial;
		}

		bool init() {
			return this->Super::initWithDefaultAssignment();
		}

		int used() {
			int used = 0;
			for(unsigned int i=0; i < FlashDriver::deviceSize; i++)
				if(Super::	isBlockBeingUsed(i))
					used++;

			return used;
		}
	};
}

TEST_GROUP(StorageManagerSimple) {
	typedef ParametricTestData<256, 2, 10, 2, 2> TestData;
	TestData* test;

	TEST_SETUP() {
		test = new TestData;
		CHECK(test->init());
	}

	TEST_TEARDOWN() {
		delete test;
	}
};

TEST(StorageManagerSimple, invalidParam) {
	CHECK(test->allocate(-test->nFileLevels-1) == TestData::FlashDriver::InvalidAddress);
	CHECK(test->allocate(test->nMetaLevels+1) == TestData::FlashDriver::InvalidAddress);
}

TEST(StorageManagerSimple, AllocateOne) {
	CHECK(test->getState(0) == TestData::BlockState::Full);
	CHECK(test->allocate(-1) == 0);
}

TEST(StorageManagerSimple, Reclaim)
{
	TestData::Address allocd[3 * TestData::FlashDriver::blockSize];

	for(unsigned int i=0; i < sizeof(allocd)/sizeof(allocd[0]); i++)
		CHECK((allocd[i] = test->allocate(-1)) != TestData::FlashDriver::InvalidAddress);

	CHECK(test->gcNeeded());

	for(unsigned int i=0; i < TestData::FlashDriver::blockSize; i++)
		test->reclaim(allocd[i]);

	CHECK(!test->gcNeeded());
}

TEST(StorageManagerSimple, InitialUsage)
{
	CHECK(test->used() == TestData::nLevels);
}

TEST(StorageManagerSimple, UsageAfterOnePage)
{
	CHECK(test->allocate(-1) != TestData::FlashDriver::InvalidAddress);
	CHECK(test->used() == TestData::nLevels);
}

TEST(StorageManagerSimple, UsageAfterOneBlock)
{
	for(unsigned int i=0; i < TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(-1) != TestData::FlashDriver::InvalidAddress);

	CHECK(test->used() == TestData::nLevels);
}

TEST(StorageManagerSimple, AllocateBlock)
{
	for(unsigned int i=0; i < TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(-1) == i);

	CHECK(test->getState(0) == TestData::BlockState::Full);
}

TEST(StorageManagerSimple, ReclaimBlock)
{
	for(unsigned int i=0; i < TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(-1) == i);

	CHECK(test->getState(0) == TestData::BlockState::Full);

	test->reclaim(0);

	CHECK(test->getState(0) == TestData::BlockState::Partial);
}

TEST(StorageManagerSimple, DontTriggerGc) {
	int level;

	for(unsigned int i=0; i < TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(0) != TestData::FlashDriver::InvalidAddress);
}

TEST(StorageManagerSimple, Iterator)
{
	TestData::FlashDriver::Address addr;
	CHECK((addr = test->allocate(-2)) != TestData::FlashDriver::InvalidAddress);
	CHECK(test->allocate(-2) != TestData::FlashDriver::InvalidAddress);

	int level;

	test->reclaim(addr);
	test->reclaim(0);

	TestData::Iterator iterator(*test);

	int last = 1, lastIdx = TestData::FlashDriver::deviceSize;
	int n=0;
	for(;iterator.currentCount(*test) != -1; iterator.step(*test)) {
		int curr = iterator.currentCount(*test);
		CHECK(curr >= last);

		if(curr == last) {
			CHECK(iterator.currentBlock() < lastIdx);
		}

		lastIdx = iterator.currentBlock();
		last = curr;
		n++;
	}

	CHECK(n == 4);

	CHECK(iterator.currentCount(*test) == -1);
	iterator.step(*test);
	CHECK(iterator.currentCount(*test) == -1);
}

TEST_GROUP(StorageManagerJustSmaller) {
	typedef ParametricTestData<256, 2, 3, 2, 2> TestData;
	TestData* test;

	TEST_SETUP() {
		test = new TestData;
	}

	TEST_TEARDOWN() {
		delete test;
	}
};

TEST(StorageManagerJustSmaller, InitFail) {
	CHECK(!test->init());
}

TEST_GROUP(StorageManagerSmallerByMore) {
	typedef ParametricTestData<256, 2, 2, 2, 2> TestData;
	TestData* test;

	TEST_SETUP() {
		test = new TestData;
	}

	TEST_TEARDOWN() {
		delete test;
	}
};

TEST(StorageManagerSmallerByMore, InitFail) {
	CHECK(!test->init());
}

TEST_GROUP(StorageManagerBarelyEnough) {
	typedef ParametricTestData<256, 2, 8, 2, 2> TestData;
	TestData* test;

	TEST_SETUP() {
		test = new TestData;
	}

	TEST_TEARDOWN() {
		delete test;
	}
};

TEST(StorageManagerBarelyEnough, AllocFail) {
	CHECK(test->init());

	for(unsigned int i=0; i < 5 * TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(-1) != TestData::FlashDriver::InvalidAddress);

	CHECK(test->allocate(-1) == TestData::FlashDriver::InvalidAddress);
}

TEST(StorageManagerBarelyEnough, AllocFailIndep) {
	CHECK(test->init());

	for(unsigned int i=0; i < 3 * TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(-1) != TestData::FlashDriver::InvalidAddress);

	for(unsigned int i=0; i < 3 * TestData::FlashDriver::blockSize; i++)
		CHECK(test->allocate(1) != TestData::FlashDriver::InvalidAddress);

	CHECK(test->allocate(-1) == TestData::FlashDriver::InvalidAddress);
	CHECK(test->allocate(1) == TestData::FlashDriver::InvalidAddress);
}
