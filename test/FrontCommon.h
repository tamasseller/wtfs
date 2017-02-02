/*
 * CommonTestUtilities.h
 *
 *  Created on: 2016.12.10.
 *      Author: tooma
 */

#ifndef COMMONTESTUTILITIES_H_
#define COMMONTESTUTILITIES_H_

#include "Wtfs.h"
#include "MockFlashDriver.h"
#include "pet/test/MockAllocator.h"

#include "util/ObjectStream.h"

#include <iostream>
#include <vector>

namespace {

template <	unsigned int bytesPerPage, unsigned int pagesPerBlock, unsigned int nBlocks,
			unsigned int buffers, unsigned int meta, unsigned int file>
struct GcTestHelpers {
	struct Config: public DefaultNolockConfig {
		typedef MockFlashDriver<bytesPerPage, pagesPerBlock, nBlocks> FlashDriver;
		typedef ::Allocator Allocator;

		static constexpr unsigned int nBuffers = buffers;
		static constexpr unsigned int maxMeta = meta;
		static constexpr unsigned int maxFile = file;
		static constexpr uint32_t maxFilenameLength = 47;
	};

	struct Fs: public Wtfs<Config> {
		typename Wtfs<Config>::Buffers inlineBuffers;
		inline Fs(bool purge=true) {
			this->bind(&inlineBuffers);
			auto x = this->initialize(purge);
			x.failed(); // Nothing to do about it
		}

		using typename Wtfs<Config>::Table;
		using typename Wtfs<Config>::Element;
		typedef typename Config::FlashDriver::Address Address ;

		unsigned int usageCounterCheck[Config::FlashDriver::deviceSize];
		void countUsage(unsigned int idx) {
			usageCounterCheck[idx]++;
		}

		struct State {
			struct Link {
				Address from, to;
				bool switchOver;

				inline Link(Address from, Address to, bool switchOver = false):
					from(from), to(to), switchOver(switchOver) {}

				bool operator==(const Link& other) {
					return from == other.from &&
							to == other.to &&
							switchOver == other.switchOver;
				}
			};

			std::vector<Link> links;

			unsigned int actualUsage[Config::FlashDriver::deviceSize];
			unsigned int registeredUsage[Config::FlashDriver::deviceSize];
			struct BlockAllocation {
				unsigned int addr, count;
			};

			BlockAllocation allocations[Wtfs<Config>::maxLevels];

			bool resembles(const State& other) {
				if(other.links.size() != links.size())
					return false;

				auto y = other.links.begin();
				for(auto x: links) {
					if(!(x == *y))
						return false;
					++y;
				}

				for(int i=0; i<Config::FlashDriver::deviceSize; i++) {
					if (actualUsage[i] != other.actualUsage[i])
						return false;
				}

				for(int i=0; i<Wtfs<Config>::maxLevels; i++) {
					if (	(other.allocations[i].count != Config::FlashDriver::blockSize) &&
							(allocations[i].addr != other.allocations[i].addr))
						return false;
				}

				return true;
			}
		};

		State gatherState() {
			using Link = typename State::Link;
			State ret;

			for(int i=0; i<sizeof(usageCounterCheck)/sizeof(usageCounterCheck[0]); i++)
				usageCounterCheck[i] = 0;

			typename WtfsTestHelper<Config>::MetaStore::ReadWriteSession session(this);
			CHECK(!this->traverse(session, [&](typename Config::FlashDriver::Address addr, unsigned int level, const typename Fs::Traversor &parents) {
				countUsage(addr / Config::FlashDriver::blockSize);

				ret.links.push_back(Link(parents.current() ? *parents.current() : -1u, addr));

				if(level == 0) {
					typename Wtfs<Config>::Buffers::Buffer temp;
					Config::FlashDriver::read(addr, &temp);

					for(int i = 0; i < ((Table*)&temp)->length(); i++) {
						Element &e = ((Table*)&temp)->elements[i];

						typename Wtfs<Config>::Node node;
						CHECK(!Fs::fetchRoot(node).failed());
						CHECK(!Fs::fetchById(node, e.key.indexed.parentId, e.key.id).failed());

						if(!node.isDirectory()) {
							CHECK(!WtfsTestHelper<Config>::traverseNode(node, [&](typename Config::FlashDriver::Address innerAddr, unsigned int level, const typename Fs::Node::Traversor &parents) {
								countUsage(innerAddr / Config::FlashDriver::blockSize);
								if(parents.current())
									ret.links.push_back(Link(parents.current()->address, innerAddr));
								else
									ret.links.push_back(Link(addr, innerAddr, true));

								return innerAddr;
							}).failed());
						}

					}
				}
				return addr;
			}).failed());

			for(int i=0; i<sizeof(usageCounterCheck)/sizeof(usageCounterCheck[0]); i++)
				ret.actualUsage[i] = usageCounterCheck[i];

			for(int i=0; i<sizeof(this->usageCounters)/sizeof(this->usageCounters[0]); i++)
				ret.registeredUsage[i] = this->usageCounters[i];

			for(int i = 0; i < sizeof(this->levelAllocations) / sizeof(this->levelAllocations[0]); i++) {
				ret.allocations[i].count = this->levelAllocations[i].usedCount;
				ret.allocations[i].addr = this->levelAllocations[i].currentAddress;
			}

			return ret;
		}

		void dump() {
			this->buffers->flush();
			State state = gatherState();

			std::cout << "digraph dump {" << std::endl;
			for(auto x: state.links) {
				if(x.from == -1u)
					std::cout << "\"root\" -> ";
				else
					std::cout << "\"" << x.from << "\" -> ";

				std::cout << "\"" << x.to << "\"";

				if(x.switchOver)
					std::cout << "[style=dotted]";

				std::cout << std::endl;
			}

			std::cout << "x[shape=record label=\"";
			for(int i = 0; i < sizeof(this->usageCounters)/sizeof(this->usageCounters[0]); i++) {
				if(i) std::cout << " | ";
				std::cout << "{" << i << "|"
						<< (int)state.registeredUsage[i] <<
						" (" << state.actualUsage[i] << ")}";
			}
			std::cout << "\"]" << std::endl;

			std::cout << "y[shape=record label=\"{level|normal}";
			for(int i = 0; i < sizeof(this->levelAllocations)/sizeof(this->levelAllocations[0]); i++) {
				std::cout << " |{" << (int)this->indexToLevel(i) << "|"
						<< (int)state.allocations[i].addr <<
						" (" << state.allocations[i].count << ")}";
			}

			std::cout << "\"]" << std::endl;
			std::cout << "x->y[style=invisible, dir=none]\n}" << std::endl;
		}
	};

	static void createFile(Fs& fs, typename Fs::Node& node, const char* name) {
		CHECK(!fs.fetchRoot(node).failed());
		CHECK(!fs.newFile(node, name, name + strlen(name)).failed());
		fs.buffers->flush();
	}

	static void removeFile(Fs& fs, typename Fs::Node& node, const char* name) {
		CHECK(!fs.fetchRoot(node).failed());
		CHECK(!fs.fetchChildByName(node, name, name + strlen(name)).failed());
		CHECK(!fs.removeNode(node).failed());
		fs.buffers->flush();
	}

	struct NodeStream {
		typename Fs::Node node;
		ObjectStream<typename Fs::Stream> stream;
		Fs &fs;
		const char* name;
		bool isOpen;

		void open(Fs &fs, const char* name) {
			CHECK(!fs.fetchRoot(node).failed());
			CHECK(!fs.fetchChildByName(node, name, name + strlen(name)).failed());
			CHECK(!fs.openStream(node, stream).failed());
			isOpen = true;
		}

		inline NodeStream(Fs &fs, const char* name, bool create = true): fs(fs), name(name), isOpen(true) {
			if(create) {
				CHECK(!fs.fetchRoot(node).failed());
				CHECK(!fs.newFile(node, name, name + strlen(name)).failed());
				CHECK(!fs.openStream(node, stream).failed());
			} else {
				open(fs, name);
			}
		}

		void close() {
			CHECK(!fs.closeStream(stream).failed());
			isOpen = false;
		}

		inline ~NodeStream() {
			if(isOpen)
				close();
		}

		void doWrite(unsigned int times) {
			while(times--)
				CHECK(!stream.writeCopy(name, strlen(name)+1).failed());

			CHECK(!stream.flush().failed());
			CHECK(!fs.flushStream(stream).failed());
			fs.buffers->flush();
		}

		void pokeWrite(unsigned int times = 1) {
			CHECK(!stream.setPosition(Fs::Stream::Start, 0).failed());
			doWrite(times);
		}

		void pokeAppend(unsigned int times = 1) {
			CHECK(!stream.setPosition(Fs::Stream::End, 0).failed());
			doWrite(times);
		}

		void pokeRead(unsigned int times = 1) {
			char buffer[strlen(name)];
			CHECK(!stream.setPosition(Fs::Stream::Start, 0).failed());

			while(times--) {
				CHECK(!stream.readCopy(buffer, strlen(name)+1).failed());
				CHECK(strcmp(buffer, name) == 0);
			}

			auto x = stream.flush();
			CHECK(!x.failed());
		}
	};
};

}



#endif /* COMMONTESTUTILITIES_H_ */
