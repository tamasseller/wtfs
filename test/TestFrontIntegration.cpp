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

#include "archive.h"
#include "archive_entry.h"

#include "CppUTest/TestHarness.h"

#include "Wtfs.h"
#include "util/Path.h"
#include "pet/test/MockAllocator.h"
#include "MockFlashDriver.h"

#include "FrontCommon.h"

#include <vector>
#include <set>
#include <algorithm>
#include <libgen.h>

TEST_GROUP(Integration) {
	using Helper = GcTestHelpers<256, 8, 537, 8, 2, 3>;
	struct Fs: public Helper::Fs {
		using Super = Helper::Fs;

		Fs(bool x=true): Super(x) {}

		void checkIntegrity() {

			struct fqid {
				int id, parent;
				std::string name;
				fqid(int id, int parent, std::string name): id(id), parent(parent), name(name) {}
				bool operator < (const fqid& other) const {return id < other.id;}
			};

			std::set<fqid> ids;
			std::set<Helper::Config::FlashDriver::Address> usedAddresses;

			typename WtfsTestHelper<Helper::Config>::MetaStore::ReadWriteSession session(this);
			CHECK(!this->Super::traverse(session, [&](Helper::Config::FlashDriver::Address addr, unsigned int level, const typename Super::Traversor &parents) {
				Super::Buffers::Buffer temp;
				Helper::Config::FlashDriver::read(addr, &temp);
				UNSIGNED_LONGS_EQUAL(level, temp.data.level);

				CHECK(usedAddresses.insert(addr).second);

				unsigned int n=0;
				for(Fs::Traversor::Iterator it = parents.iterator(); it.current(); it.step())
					n++;

				CHECK(level + n == levels);

				if(level == 0) {
					unsigned int n = ((Table*)&temp)->length();
					while(n--) {
						Element &e = ((Table*)&temp)->elements[n];
						CHECK(ids.insert(fqid(e.key.id, e.key.indexed.parentId, std::string(e.key.name))).second);
					}
				}

				return addr;
			}).failed());

			for(auto x: ids) {
				Node node;
				CHECK(!fetchRoot(node).failed());
				CHECK(!fetchById(node, x.parent, x.id).failed());

				if(!node.isDirectory()) {
					CHECK(!WtfsTestHelper<Helper::Config>::traverseNode(node, [&](Helper::Config::FlashDriver::Address addr, unsigned int level, const Fs::Node::Traversor &parents) {
						Buffers::Buffer temp;
						Helper::Config::FlashDriver::read(addr, &temp);
						CHECK(usedAddresses.insert(addr).second);
						UNSIGNED_LONGS_EQUAL(-1-level, temp.data.level);
						return addr;
					}).failed());
				}
			}
		}
	};

	TEST_SETUP() {
		INHIBIT_FAILURE_INJECTION_FOR_TESTCASE();
		mock().disable();
	}

	class TarGzReader {
		const char* name;
		struct archive *archive;

		public:
			struct TarGzEventHandler {
				virtual void directory(TarGzReader*, const char*) = 0;
				virtual void file(TarGzReader*, const char*, int) = 0;
			};

			int readData(char* buff, int buffsize) {
				return archive_read_data(archive, buff, buffsize);
			}

			TarGzReader(const char* name): name(name), archive(0){}

			void iterate(TarGzEventHandler* handler) {
				archive = archive_read_new();
				archive_read_support_filter_all(archive);
				archive_read_support_format_all(archive);

				if (archive_read_open_filename(archive, name, 10240) != ARCHIVE_OK)
					perror("error: testData.tar.gz\n");

				for(struct archive_entry *entry; archive_read_next_header(archive, &entry) == ARCHIVE_OK;) {
					ssize_t size = archive_entry_size(entry);

					if(size)
						handler->file(this, archive_entry_pathname(entry), size);
					else
						handler->directory(this, archive_entry_pathname(entry));
				}

				if (archive_read_free(archive) != ARCHIVE_OK)
					printf("wtf libarchive error\n");

			}

	};

	void checkListing(Fs &fs, const Path<Fs, Allocator>& path, std::initializer_list<const char*> reqContents)
	{
		Fs::Node node;
		std::vector<std::string> contents;

		pet::GenericError res = fs.fetchFirstChild(node);

		if(res.failed())
			return;

		do {
			const char *from, *to;
			node.getName(from, to);
			contents.push_back(std::string(from, to-from));

			pet::GenericError res = fs.fetchFirstChild(node);

			if(res.failed())
				return;

		} while(res);

		std::sort(contents.begin(), contents.end());

		std::vector<std::string>::iterator it = contents.begin();

		for(const char *x: reqContents){
			CHECK(strcmp((*it).c_str(), x) == 0);
			++it;
		}

		CHECK(it == contents.end());
	}
};

TEST(Integration, Replicate) {
	Path<Fs, Allocator> path;

	TarGzReader tgzReader("testData.tar.gz");

	struct ReplicatorBase: public TarGzReader::TarGzEventHandler {
		inline virtual ~ReplicatorBase(){}
		Fs& fs;
		Fs::Node& node;
		Path<Fs, Allocator> &path;

		inline virtual void handleFile(TarGzReader* reader, const char* name, unsigned int size) {}
		inline virtual void handleDir(const char* name) {}

		virtual void directory(TarGzReader* reader, const char* pathName) {
			static char dir[128], base[128], name[128];
			strcpy(name, pathName);
			strcpy(base, basename(name));
			strcpy(dir, dirname(name));
			path.clear();
			CHECK(!path.append(fs, node, dir).failed());
			handleDir(base);
		}

		virtual void file(TarGzReader* reader, const char* pathName, int size) {
			static char dir[128], base[128], name[128];
			strcpy(name, pathName);
			strcpy(base, basename(name));
			strcpy(dir, dirname(name));
			path.clear();
			CHECK(!path.append(fs, node, dir).failed());
			handleFile(reader, base, size);
		}

		inline ReplicatorBase(Fs& fs, Fs::Node& node, Path<Fs, Allocator> &path): fs(fs), node(node), path(path){}
	};

	struct Copier: public ReplicatorBase {
		inline virtual ~Copier(){}
		virtual void handleDir(const char* name) {
			CHECK(!fs.newDirectory(node, name, name+strlen(name)).failed());
		}

		virtual void handleFile(TarGzReader* reader, const char* name, unsigned int size) {
			Fs::Stream stream;
			CHECK(!fs.newFile(node, name, name+strlen(name)).failed());
			CHECK(!fs.openStream(node, stream).failed());

			while(1) {
				void *buff;
				pet::GenericError res = stream.write(buff, size);
				CHECK(!res.failed());

				if(res.failed())
					return;

				unsigned int writable = res;
				while(writable) {
					ssize_t readSize = reader->readData((char*)buff, writable);
					writable -= readSize;
					buff = (uint8_t*)buff + readSize;
				}

				size -= res;
				if (!size)
					break;

			}

			CHECK(!fs.closeStream(stream).failed());
			fs.buffers->flush();
			fs.checkIntegrity();
		}

		inline Copier(Fs& fs, Fs::Node& node, Path<Fs, Allocator> &path): ReplicatorBase(fs, node, path){}
	};

	struct Checker: public ReplicatorBase {
		inline virtual ~Checker(){}
		virtual void handleDir(const char* name) {
			CHECK(!path.append(fs, node, name).failed());
		}

		virtual void handleFile(TarGzReader* reader, const char* name, unsigned int size) {
			Fs::Stream stream;
			CHECK(!path.append(fs, node, name).failed());
			CHECK(!fs.openStream(node, stream).failed());

			while(1) {
				void *wtfsBuff;
				pet::GenericError res = stream.read(wtfsBuff, size);

				if(res.failed() || !res)
					return;

				unsigned int readable = res;
				char* tgzBuffStart = new char[readable];
				char* tgzBuffEnd = tgzBuffStart;
				while(readable) {
					ssize_t readSize = reader->readData(tgzBuffEnd, readable);
					readable -= readSize;
					tgzBuffEnd += readSize;
				}

				CHECK(memcmp(tgzBuffStart, wtfsBuff, res) == 0);

				delete[] tgzBuffStart;

				size -= res;
				if (!size)
					break;

			}

			CHECK(!fs.closeStream(stream).failed());
		}

		inline Checker(Fs& fs, Fs::Node& node, Path<Fs, Allocator> &path): ReplicatorBase(fs, node, path){}
	};

	struct Rewriter: public ReplicatorBase {
		inline virtual ~Rewriter(){}

		virtual void handleFile(TarGzReader* reader, const char* name, unsigned int size) {
			Fs::Stream stream;
			CHECK(!path.append(fs, node, name).failed());
			CHECK(!fs.openStream(node, stream).failed());

			while(1) {
				void *wtfsBuff;
				pet::GenericError res = stream.write(wtfsBuff, size);

				if(res.failed())
					return;

				unsigned int readable = res;
				while(readable--) {
					*(uint8_t*)wtfsBuff = ~*(uint8_t*)wtfsBuff;
					wtfsBuff = (uint8_t*)wtfsBuff + 1;
				}

				size -= res;
				if (!size)
					break;
			}

			CHECK(!fs.closeStream(stream).failed());
		}

		inline Rewriter(Fs& fs, Fs::Node& node, Path<Fs, Allocator> &path): ReplicatorBase(fs, node, path){}
	};

	struct Deleter: public ReplicatorBase {
		inline virtual ~Deleter(){}
		std::vector<std::string> dirs;

		virtual void directory(TarGzReader* reader, const char* pathName) {
			dirs.push_back(pathName);
		}

		virtual void file(TarGzReader* reader, const char* pathName, int size) {
			dirs.push_back(pathName);
		}

		void work() {
			std::sort(dirs.rbegin(), dirs.rend());
			for(auto name: dirs) {
				path.clear();
				CHECK(!path.append(fs, node, name.c_str()).failed());
				CHECK(!fs.removeNode(node).failed());
			}
		}

		inline Deleter(Fs& fs, Fs::Node& node, Path<Fs, Allocator> &path): ReplicatorBase(fs, node, path){}
	};


	Fs::Node node;
	{
		Fs fs;
		Copier copier(fs, node, path);
		Checker checker(fs, node, path);
		Rewriter rewriter(fs, node, path);
		CHECK(!path.append(fs, node, "/").failed());
		checkListing(fs, path, {});
		tgzReader.iterate(&copier);
		tgzReader.iterate(&checker);
		tgzReader.iterate(&rewriter);
		fs.buffers->flush();
	}

	{
		Fs fs(false);
		Checker checker2(fs, node, path);
		Rewriter rewriter2(fs, node, path);
		Deleter deleter(fs, node, path);
		tgzReader.iterate(&rewriter2);
		tgzReader.iterate(&checker2);
		tgzReader.iterate(&deleter);
		deleter.work();
		checkListing(fs, path, {});

		Copier copier(fs, node, path);
		Checker checker(fs, node, path);
		tgzReader.iterate(&copier);
		tgzReader.iterate(&checker);
	}
}

