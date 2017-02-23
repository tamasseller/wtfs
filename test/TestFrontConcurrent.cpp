/*
 * Concurrent.cpp
 *
 *  Created on: 2016.12.17.
 *      Author: tooma
 */

#include <vector>
#include <string>
#include <cstring>

#include "Wtfs.h"
#include "util/ObjectStream.h"
#include "FrontPlainDummies.h"

namespace {
typedef PlainDummyFlashDriver<256, 4, 10> FlashDriver;

struct PthreadMutexWrapper {
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	void lock() {
		pthread_mutex_lock(&mutex);
	}

	void unlock() {
		pthread_mutex_unlock(&mutex);
	}
};

struct PthreadRWLockWrapper {
	pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

	void rlock() {
		pthread_rwlock_rdlock(&lock);
	}

	void runlock() {
		pthread_rwlock_unlock(&lock);
	}

	void wlock() {
		pthread_rwlock_wrlock(&lock);
	}

	void wunlock() {
		pthread_rwlock_unlock(&lock);
	}
};

struct Config: public DefaultRwlockConfig<PthreadMutexWrapper, PthreadRWLockWrapper> {
	typedef ::PlainDummyAllocator Allocator;
	typedef ::FlashDriver FlashDriver;
	static constexpr unsigned int nBuffers = 5;
	static constexpr unsigned int maxMeta = 2;
	static constexpr unsigned int maxFile = 2;
};

struct TestBase {
	typedef Wtfs<Config> Fs;
	Fs::Buffers buffers;
	Fs fs;

	bool bad = false;

	void requireNoError(pet::GenericError &&result) {
		if(result.failed())
			bad = true;
	}

	void createFile(Fs::Node &node, const char* name) {
		requireNoError(fs.fetchRoot(node));
		requireNoError(fs.newFile(node, name, name+strlen(name)));
	}

	template<class Operation, class... Ops>
	void runNParallel(unsigned int n, Ops... ops) {
		std::vector<pthread_t*> threads;

		for(int i=0; i < n; i++) {
			pthread_t* t = (pthread_t*)malloc(sizeof(pthread_t));
			pthread_create(t, NULL, &Operation::start, (void*)(new Operation(i, ops...)));
			threads.push_back(t);
		}

		for(auto x: threads) {
			pthread_join(*x, 0);
			delete x;
		}
	}
};

}

struct SlightlyConcurrentTest: public TestBase  {
	bool run (){
		fs.bind(&buffers);
		fs.initialize(true);

		Fs::Node temp;
		createFile(temp, "dummy1");
		createFile(temp, "dummy2");
		createFile(temp, "dummy3");
		createFile(temp, "dummy4");
		createFile(temp, "dummy5");

		class Lambda {
			SlightlyConcurrentTest &self;
			Fs::Node node;
			ObjectStream<Fs::Stream> stream;
			void run() {
				const char *nameStart, *nameEnd;
				node.getName(nameStart, nameEnd);

				const auto times = Config::FlashDriver::pageSize / (nameEnd-nameStart);
				for(int i=0; i<times; i++) {
					self.requireNoError(stream.writeCopy(nameStart, nameEnd-nameStart));
				}

				self.fs.closeStream(stream);
			}
		public:
			Lambda(int i, SlightlyConcurrentTest &self): self(self) {
				const char* name = (std::string("test") + std::to_string(i)).c_str();
				self.requireNoError(self.fs.fetchRoot(node));
				self.requireNoError(self.fs.newFile(node, name, name+strlen(name)));
				self.requireNoError(self.fs.openStream(node, stream));
			}
			static void* start(void *self) {((Lambda*)self)->run(); return 0;}
		};

		runNParallel<Lambda>(3, *this);

		return bad;
	}
};

struct MoreConcurrentTest: public TestBase  {
	bool run (){
		fs.bind(&buffers);
		fs.initialize(true);

		class Lambda {
			MoreConcurrentTest &self;
			char name[64];
			void run() {
				Fs::Node node;
				ObjectStream<Fs::Stream> stream;

				self.requireNoError(self.fs.fetchRoot(node));
				self.requireNoError(self.fs.newFile(node, name, name+strlen(name)));
				self.requireNoError(self.fs.openStream(node, stream));

				const auto times = Config::FlashDriver::pageSize / strlen(name);
				for(int i=0; i<times; i++) {
					self.requireNoError(stream.writeCopy(name, strlen(name)));
				}

				self.fs.closeStream(stream);
			}
		public:
			Lambda(int i, MoreConcurrentTest &self): self(self) {
				std::strcpy(name, (std::string("dummy") + std::to_string(i)).c_str());
			}
			static void* start(void *self) {((Lambda*)self)->run(); return 0;}
		};

		runNParallel<Lambda>(5, *this);
		return bad;
	}
};

struct StreamConcurrentTest: public TestBase  {
	bool run (){
		fs.bind(&buffers);
		fs.initialize(true);

		Fs::Node node;
		createFile(node, "foo");

		class Lambda {
			StreamConcurrentTest &self;
			const char *data = "bar";
			ObjectStream<Fs::Stream> stream;

			void run() {
				const auto times = Config::FlashDriver::pageSize / strlen(data);

				for(int n=0; n<100; n++) {
					for(int i=0; i<times; i++) {
						self.requireNoError(stream.writeCopy(data, strlen(data)));
					}

					stream.setPosition(Fs::Stream::Start, 0);
				}

				self.fs.closeStream(stream);
			}
		public:
			Lambda(int i, StreamConcurrentTest &self, Fs::Node &node): self(self) {
				self.requireNoError(self.fs.openStream(node, stream));
			}
			static void* start(void *self) {((Lambda*)self)->run(); return 0;}
		};

		runNParallel<Lambda>(15, *this, node);
		return bad;
	}
};

int main()
{
	SlightlyConcurrentTest slightlyConcurrentTest;
	slightlyConcurrentTest.run();

	MoreConcurrentTest moreConcurrentTest;
	moreConcurrentTest.run();

	StreamConcurrentTest streamConcurrentTest;
	streamConcurrentTest.run();
}
