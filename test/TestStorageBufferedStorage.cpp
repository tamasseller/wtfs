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

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "MockFlashDriver.h"

#include "storage/BufferedStorage.h"
#include "front/ConfigHelpers.h"

typedef MockFlashDriver<1, 1, 1> FlashDriver;

namespace {
	struct MockStorageManager {
		unsigned int addr=0;
		FlashDriver::Address allocate(int level) {
			unsigned int ret = addr++;
			mock("StorageManager").actualCall("allocate").withIntParameter("level", level);
			return ret;
		}

		void reclaim(FlashDriver::Address addr) {
			mock("StorageManager").actualCall("reclaim").withIntParameter("addr", addr);
		}
	};

	typedef BufferedStorage<FlashDriver, MockStorageManager, DefaultNolockConfig::Mutex, 2> MockedBufferedStorage;

	struct TestData: private MockedBufferedStorage::Initializer {
		MockedBufferedStorage storage;
		MockStorageManager manager;
		TestData() {
			MockedBufferedStorage::Initializer::initialize(&storage, &manager);
		}
	};
}

TEST_GROUP(BufferedStorageEmpty) {
	TestData* test;

	TEST_SETUP() {
		test = new TestData;
	}

	TEST_TEARDOWN() {
		mock().checkExpectations();
		mock().clear();
		delete test;
	}
};

TEST(BufferedStorageEmpty, Exhaust) {
	CHECK(test->storage.find(FlashDriver::InvalidAddress) != 0);
	CHECK(test->storage.find(FlashDriver::InvalidAddress) != 0);
	CHECK(test->storage.find(FlashDriver::InvalidAddress) == 0);
}

TEST(BufferedStorageEmpty, read) {
	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 12);
	MockedBufferedStorage::Buffer* buffer = test->storage.find(12);
	buffer->data.level = 23;
	test->storage.release(buffer, BufferReleaseCondition::Clean);
}

TEST(BufferedStorageEmpty, writeNewAlloc) {
	MockedBufferedStorage::Buffer* buffer = test->storage.find(FlashDriver::InvalidAddress);
	buffer->data.level = 23;
	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 23);
	test->storage.release(buffer, BufferReleaseCondition::Dirty);
}

TEST(BufferedStorageEmpty, writeNewFlushWrite) {
	MockedBufferedStorage::Buffer* buffer = test->storage.find(FlashDriver::InvalidAddress);
	buffer->data.level = 23;

	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 23);
	test->storage.release(buffer, BufferReleaseCondition::Dirty);

	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	test->storage.flush();
}

TEST(BufferedStorageEmpty, concurrentReadValidSame) {
	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 12);
	MockedBufferedStorage::Buffer* buffer1 = test->storage.find(12);
	MockedBufferedStorage::Buffer* buffer2 = test->storage.find(12);
	CHECK(buffer1 == buffer2);
}

TEST(BufferedStorageEmpty, concurrentReadInvalidDifferent) {
	MockedBufferedStorage::Buffer* buffer1 = test->storage.find(FlashDriver::InvalidAddress);
	MockedBufferedStorage::Buffer* buffer2 = test->storage.find(FlashDriver::InvalidAddress);
	CHECK(buffer1 != buffer2);
}

TEST(BufferedStorageEmpty, writingTwiceResultsInOneActualAllocPlusWrite) {
	MockedBufferedStorage::Buffer* buffer = test->storage.find(FlashDriver::InvalidAddress);
	buffer->data.level = 23;

	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 23);
	test->storage.release(buffer, BufferReleaseCondition::Dirty);

	buffer = test->storage.find(0);
	test->storage.release(buffer, BufferReleaseCondition::Dirty);

	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	test->storage.flush();
}

TEST(BufferedStorageEmpty, updateReclaim) {
	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 123);
	MockedBufferedStorage::Buffer* buffer = test->storage.find(123);
	buffer->data.level = 45;

	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 45);
	mock("StorageManager").expectOneCall("reclaim").withIntParameter("addr", 123);
	test->storage.release(buffer, BufferReleaseCondition::Dirty);

	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	test->storage.flush();
}

TEST(BufferedStorageEmpty, dispose) {
	MockedBufferedStorage::Buffer* buffer = test->storage.find(FlashDriver::InvalidAddress);
	buffer->data.level = 45;

	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 45);
	test->storage.release(buffer, BufferReleaseCondition::Dirty);

	buffer = test->storage.find(0);

	mock("StorageManager").expectOneCall("reclaim").withIntParameter("addr", 0);
	test->storage.release(buffer, BufferReleaseCondition::Purge);

	test->storage.flush();
}

TEST(BufferedStorageEmpty, evictCleanForRead) {
	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 100);
	MockedBufferedStorage::Buffer* buffer1 = test->storage.find(100);
	buffer1->data.level = 1;
	test->storage.release(buffer1, BufferReleaseCondition::Clean);

	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 200);
	MockedBufferedStorage::Buffer* buffer2 = test->storage.find(200);
	buffer2->data.level = 2;
	test->storage.release(buffer2, BufferReleaseCondition::Clean);

	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 300);
	MockedBufferedStorage::Buffer* buffer3 = test->storage.find(300);
	buffer3->data.level = 3;
	test->storage.release(buffer3, BufferReleaseCondition::Clean);

	CHECK(buffer1 != buffer2);

	test->storage.flush();
}

TEST(BufferedStorageEmpty, evictCleanForEmpty) {
	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 100);
	MockedBufferedStorage::Buffer* buffer1 = test->storage.find(100);
	buffer1->data.level = 1;
	test->storage.release(buffer1, BufferReleaseCondition::Clean);

	mock("FlashDriver").expectOneCall("read").withIntParameter("addr", 200);
	MockedBufferedStorage::Buffer* buffer2 = test->storage.find(200);
	buffer2->data.level = 2;
	test->storage.release(buffer2, BufferReleaseCondition::Clean);

	MockedBufferedStorage::Buffer* buffer3 = test->storage.find(FlashDriver::InvalidAddress);
	buffer3->data.level = 3;
	test->storage.release(buffer3, BufferReleaseCondition::Clean);

	CHECK(buffer1 != buffer2);

	test->storage.flush();
}

TEST_GROUP(BufferedStorageFull) {
	TestData* test;
	MockedBufferedStorage::Buffer *buffer1, *buffer2;

	TEST_SETUP() {
		test = new TestData;

		buffer1 = test->storage.find(FlashDriver::InvalidAddress);
		buffer1->data.level = 1;
		mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 1);
		test->storage.release(buffer1, BufferReleaseCondition::Dirty);

		buffer2 = test->storage.find(FlashDriver::InvalidAddress);
		buffer2->data.level = 2;
		mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 2);
		test->storage.release(buffer2, BufferReleaseCondition::Dirty);
	}

	TEST_TEARDOWN() {
		mock().checkExpectations();
		mock().clear();
		delete test;
	}
};

TEST(BufferedStorageFull, flushOrderSimple) {
	mock().strictOrder();
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 1);
	test->storage.flush();
}

TEST(BufferedStorageFull, flushOrderChanged) {
	MockedBufferedStorage::Buffer* buffer3 = test->storage.find(0);
	CHECK(buffer3 == buffer1);
	test->storage.release(buffer1, BufferReleaseCondition::Dirty);

	mock().strictOrder();
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 1);
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	test->storage.flush();
}

TEST(BufferedStorageFull, dirtyEvictionSimple) {
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	buffer2 = test->storage.find(FlashDriver::InvalidAddress);
}

TEST(BufferedStorageFull, dirtyEvictionOther) {
	test->storage.release(test->storage.find(0), BufferReleaseCondition::Dirty);

	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 1);
	buffer2 = test->storage.find(FlashDriver::InvalidAddress);
}

TEST(BufferedStorageFull, moreRecentCleanChoosen) {
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 1);
	test->storage.flush();

	MockedBufferedStorage::Buffer* buffer3 = test->storage.find(0);

	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 1);
	mock("StorageManager").expectOneCall("reclaim").withIntParameter("addr", 0);
	test->storage.release(buffer3, BufferReleaseCondition::Dirty);
	test->storage.release(test->storage.find(1), BufferReleaseCondition::Clean);
	test->storage.find(FlashDriver::InvalidAddress);
}

TEST(BufferedStorageFull, veryOldDirtyChoosen) {
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 0);
	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 1);
	test->storage.flush();

	MockedBufferedStorage::Buffer* buffer3 = test->storage.find(0);

	mock("StorageManager").expectOneCall("allocate").withIntParameter("level", 1);
	mock("StorageManager").expectOneCall("reclaim").withIntParameter("addr", 0);
	test->storage.release(buffer3, BufferReleaseCondition::Dirty);
	test->storage.release(test->storage.find(1), BufferReleaseCondition::Clean);
	test->storage.release(test->storage.find(1), BufferReleaseCondition::Clean);

	mock("FlashDriver").expectOneCall("write").withIntParameter("addr", 2);
	test->storage.find(FlashDriver::InvalidAddress);
}
