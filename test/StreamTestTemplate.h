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

#ifndef STREAMTESTTEMPLATE_H_
#define STREAMTESTTEMPLATE_H_

#include <cstring>

#include "util/ObjectStream.h"

#include "pet/test/MockAllocator.h"

template<class Fs>
struct FsStreamTestTemplate {
	struct StreamTestData {
		Fs fs;
		typename Fs::Node node;
		ObjectStream<typename Fs::Stream> stream;

		inline StreamTestData() {
			const char* name = "foo";
			CHECK(!fs.fetchRoot(node).failed());
			CHECK(!fs.newFile(node, name, name + strlen(name)).failed());
			CHECK(!fs.openStream(node, stream).failed());
		}

		void teardown() {
			CHECK(!fs.flushStream(stream).failed());
			pet::FailureInjector::disable();
			CHECK_ALWAYS(FailableAllocator::allFreed());
			CHECK(!fs.removeNode(node).failed());
			pet::FailureInjector::enable();
		}

		void write(const char* data, unsigned int length, ObjectStream<typename Fs::Stream> *stream=0) {
			if(!stream)
				stream = &this->stream;

			pet::GenericError ret = stream->writeCopy(data, length);
			CHECK(!ret.failed());
			CHECK(length == ret);
		}

		void read(const char* data, unsigned int length, ObjectStream<typename Fs::Stream> *stream=0) {
			if(!stream)
				stream = &this->stream;

			char* buffer = new char[length];

			pet::GenericError ret = stream->readCopy(buffer, length);
			CHECK(!ret.failed());
			if(!ret.failed()) {
				CHECK(length == ret);
				CHECK(memcmp(data, buffer, length) == 0);
			}

			delete[] buffer;
		}
	};

	struct StreamEmpty {
		StreamTestData *test;

		inline void setup() {
			test = new StreamTestData;
		}

		inline void teardown() {
			test->teardown();
			delete test;
		}

		inline void readZero() {
			pet::GenericError ret = test->stream.readCopy(0, 0);
			CHECK(!ret.failed());
			CHECK(ret == 0);
		}

		inline void readOne() {
			char c;
			pet::GenericError ret = test->stream.readCopy(&c, 1);
			CHECK(!ret.failed());
			CHECK(ret == 0);
		}

		inline void writeZero() {
			CHECK_ALWAYS(!test->stream.writeCopy(0, 0).failed());
		}

		inline void writeOne() {
			char c = 'a';
			test->write(&c, 1);
		}

		inline void writeMore() {
			const char c[] = "StreamLipsum";
			test->write(c, sizeof(c));
		}

		inline void writeMuch() {
			const char c[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
			test->write(c, sizeof(c));
		}

		inline void seekyModify() {
			const unsigned int N = 12*12+1;						/* create 4byte sequence data */
			static unsigned int data[N];
			for(unsigned int i = 0; i<N; i++)
				data[i] = i;

			static int temp = 0;
			pet::GenericError ret1 = test->stream.writeCopy((const char*)&temp, (unsigned int)sizeof(temp));
			CHECK(!ret1.failed());
			if(ret1.failed())
				return;

			while(1) {
				pet::GenericError ret2 = test->stream.setPosition(Fs::Stream::Current, -(unsigned int)sizeof(temp));
				CHECK(!ret2.failed());
				if(ret2.failed()) {
					return;
				}

				pet::GenericError ret3 = test->stream.readCopy((char*)&temp, (unsigned int)sizeof(temp));
				CHECK(!ret3.failed());
				if(ret3.failed())
					return;

				temp++;
				if(temp == N)
					break;


				pet::GenericError ret4 = test->stream.writeCopy((char*)&temp, (unsigned int)sizeof(temp));
				CHECK(!ret4.failed());
				if(ret4.failed())
					return;
			}

			if(test->fs.flushStream(test->stream).failed()) {
				FAIL("Unexpected failure");
				return;
			}

			CHECK(test->stream.getSize() == sizeof(data));
			if(test->stream.setPosition(Fs::Stream::Start, 0).failed()) {
				FAIL("Unexpected failure");
				return;
			}

			test->read((const char*)data, (unsigned int)sizeof(data));
		}
	};

	static constexpr const char lipsum[] = "Lorem ipsum dolor sit amet.";

	struct StreamLipsum {
		StreamTestData *test;

		inline void setup() {
			pet::FailureInjector::disable();
			test = new StreamTestData;
			test->write(lipsum, sizeof(lipsum));
			pet::FailureInjector::enable();
		}

		inline void teardown() {
			test->teardown();
			delete test;
		}

		inline void read() {
			auto ret = test->stream.setPosition(Fs::Stream::Start, 0);
			CHECK(!ret.failed());
			if(!ret.failed())
				test->read(lipsum, sizeof(lipsum));
		}

		inline void absSeekRead() {
			auto ret = test->stream.setPosition(Fs::Stream::Start, 3);
			CHECK(!ret.failed());
			if(!ret.failed())
				test->read(lipsum+3, sizeof(lipsum)-3);

		}

		inline void relSeekReadDifferent() {
			auto ret = test->stream.setPosition(Fs::Stream::Current, -5);
			CHECK(!ret.failed());
			if(!ret.failed())
				test->read(lipsum+sizeof(lipsum)-5, 5);
		}

		inline void endSeekReadDifferent() {
			auto ret = test->stream.setPosition(Fs::Stream::End, -5);
			CHECK(!ret.failed());
			if(!ret.failed())
				test->read(lipsum+sizeof(lipsum)-5, 5);
		}

		inline void relSeekReadSame() {
			auto ret = test->stream.setPosition(Fs::Stream::Current, -3);
			CHECK(!ret.failed());
			if(!ret.failed())
				test->read(lipsum+sizeof(lipsum)-3, 3);
		}

		inline void endSeekReadSame() {
			auto ret = test->stream.setPosition(Fs::Stream::End, -3);
			CHECK(!ret.failed());
			if(!ret.failed())
				test->read(lipsum+sizeof(lipsum)-3, 3);
		}
	};


	struct StreamConcurrent {
		StreamTestData *test;
		ObjectStream<typename Fs::Stream> otherStream;

		inline void setup() {
			test = new StreamTestData;
			CHECK(!test->fs.openStream(test->node, otherStream).failed());
		}

		inline void teardown() {
			test->teardown();
			delete test;
		}

		inline void writeRead() {
			test->write(lipsum, sizeof(lipsum), &otherStream);
			CHECK(!otherStream.flush().failed());
			test->read(lipsum, sizeof(lipsum));
		}

		inline void writeWriteRead() {
			test->write(lipsum, sizeof(lipsum), &otherStream);
			CHECK(!otherStream.flush().failed());

			auto ret = test->stream.setPosition(Fs::Stream::End, 0);
			CHECK(!ret.failed());
			test->write(lipsum, sizeof(lipsum));
			CHECK(!test->stream.flush().failed());

			test->read(lipsum, sizeof(lipsum), &otherStream);
		}
	};
};

template<class Fs>
constexpr const char FsStreamTestTemplate<Fs>::lipsum[];

#define FS_STREAM_TEST_TEMPLATE(x)												\
TEST_GROUP(StreamEmpty ## x) {													\
	typename FsStreamTestTemplate<x>::StreamEmpty test;							\
	TEST_SETUP() { test.setup(); }												\
	TEST_TEARDOWN() { test.teardown(); }                                        \
};																				\
TEST(StreamEmpty ## x, ReadZero) { test.readZero(); }                           \
TEST(StreamEmpty ## x, ReadOne) { test.readOne(); }                             \
TEST(StreamEmpty ## x, WriteZero) { test.writeZero(); }                         \
TEST(StreamEmpty ## x, WriteOne) { test.writeOne(); }							\
TEST(StreamEmpty ## x, WriteMore) { test.writeMore(); }                         \
TEST(StreamEmpty ## x, WriteMuch) { test.writeMuch(); }                         \
TEST(StreamEmpty ## x, SeekyModify) { test.seekyModify(); }                     \
TEST_GROUP(StreamLipsum ## x) {                                                 \
	typename FsStreamTestTemplate<x>::StreamLipsum test;                        \
	TEST_SETUP() { test.setup(); }                                              \
	TEST_TEARDOWN() { test.teardown(); }                                        \
};																				\
TEST(StreamLipsum ## x, Read) { test.read(); }                                  \
TEST(StreamLipsum ## x, AbsSeekRead) { test.absSeekRead(); }                    \
TEST(StreamLipsum ## x, RelSeekReadDifferent) { test.relSeekReadDifferent(); }  \
TEST(StreamLipsum ## x, EndSeekReadDifferent) { test.endSeekReadDifferent(); }  \
TEST(StreamLipsum ## x, RelSeekReadSame) { test.relSeekReadSame(); }            \
TEST(StreamLipsum ## x, EndSeekReadSame) { test.endSeekReadSame(); }            \
TEST_GROUP(StreamConcurrent ## x) {                                             \
	typename FsStreamTestTemplate<x>::StreamConcurrent test;                    \
	TEST_SETUP() { test.setup(); }                                              \
	TEST_TEARDOWN() { test.teardown(); }                                        \
};                                                                              \
TEST(StreamConcurrent ## x, WriteRead) { test.writeRead(); }                    \
TEST(StreamConcurrent ## x, WriteWriteRead) { test.writeWriteRead(); }

#endif /* STREAMTESTTEMPLATE_H_ */
