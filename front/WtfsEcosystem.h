/*******************************************************************************
 *
 * Copyright (c) 2016, 2017 Seller Tamás. All rights reserved.
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

#ifndef WTFSECOSYSTEM_H_
#define WTFSECOSYSTEM_H_

#include <cstdint>

#include "ubiquitous/Error.h"
#include "ubiquitous/Trace.h"

#include "data/LinkedList.h"

#include "btree/BTree.h"
#include "blob/BlobTree.h"
#include "MetaKeys.h"

#include "storage/BufferedStorage.h"
#include "storage/StorageManager.h"
#include "Storage.h"
#include "ConfigHelpers.h"

class WtfsTrace: public ubiq::Trace<WtfsTrace> {};

template<class T>
class WtfsTestHelper;

template<class Config>
class WtfsEcosystem {
	friend WtfsTestHelper<Config>;
public:
	class WtfsMain;
private:
	class FileTree;
	class Node;
	typedef uint32_t NodeId;

	typedef typename Config::FlashDriver FlashDriver;
	typedef typename Config::Allocator Allocator;
	typedef typename Config::RWLock RWLock;
	typedef typename Config::Mutex Mutex;
	typedef MetaFullKey<Config::maxFilenameLength> FullKey;
	typedef MetaIndexKey<Config::maxFilenameLength> IndexKey;
	typedef BufferedStorage<FlashDriver, WtfsMain, Mutex, Config::nBuffers> Buffers;
	typedef StorageManager<FlashDriver, Config::maxMeta, Config::maxFile> Manager;
	typedef MetaStorage<FlashDriver, Allocator, WtfsMain, Buffers> MetaStore;
	typedef BlobStorage<FlashDriver, Allocator, WtfsMain, Buffers, Node> BlobStore;
	typedef BTree<MetaStore, FullKey, IndexKey, FileTree, Allocator> MetaTree;

	struct FileTree: public BlobTree<BlobStore, Allocator> {
		FileTree(): BlobTree<BlobStore, Allocator>(BlobStore::InvalidAddress, -1u) {}

		void initialize(bool isValid) {
			this->size = ((isValid) ? (0) : (-1u));
			this->root = BlobStore::InvalidAddress;
		}

		bool hasData() {
			return this->size != -1u;
		}
	};

	class Node: public FileTree {
		friend WtfsTestHelper<Config>;
		friend WtfsMain;
		friend BlobStore;
		friend container::LinkedList<Node>;
		FullKey key;
		WtfsMain* fs;
		bool dirty;

		Node* next = (Node*)-1u;
		uint32_t referenceCount = 0;
	public:
		inline Node(): fs(0), dirty(false), next(0) {key.id=-1u;}
		inline void getName(const char*&, const char*&);
		inline bool isDirectory();
		inline NodeId getId();
	};

public:
	class WtfsMain: public MetaTree, public Manager, private Buffers::Initializer, protected RWLock {
	public:
		typedef typename WtfsEcosystem::Buffers Buffers;

		template<class BackendConfig, class Allocator, class Child>
		friend class StorageBase;
		friend MetaStore;

		class Stream;
		typedef typename WtfsEcosystem::NodeId NodeId;
		typedef typename WtfsEcosystem::Node Node;
		typedef typename MetaTree::Table MetaTable ;
		typedef typename MetaTree::Element MetaElement ;

		Buffers *buffers = 0;
	private:
		Mutex maxIdLock;
		uint32_t maxId = 1;
		uint32_t updateCounter = 1;

		Mutex nodeListLock;
		container::LinkedList<Node> openNodes;
		bool inGc = false;
		bool isReadonly = false;
		WtfsEcosystem::Node tempNode;

		template<bool isDir>
		inline ubiq::GenericError addNew(Node&, const char*, const char*);

		inline ubiq::GenericError moveAroundMetaPages(typename FlashDriver::Address const page, uint32_t usedPages);
		inline ubiq::GenericError moveAroundBlobPages(typename FlashDriver::Address const page, uint32_t usedPages);
		inline ubiq::GenericError collectGarbage();
	protected:
		inline ubiq::GenericError fetchById(Node& node, NodeId parent, NodeId id);
	public:
		inline void bind(Buffers*);
		ubiq::GenericError initialize(bool purge=false);
		ubiq::GenericError fetchRoot(Node&);
		ubiq::GenericError fetchChildByName(Node&, const char*, const char*);
		ubiq::GenericError fetchChildById(Node&, NodeId);
		ubiq::GenericError fetchFirstChild(Node&);
		ubiq::GenericError fetchNextSibling(Node&);
		ubiq::GenericError newDirectory(Node&, const char*, const char*);
		ubiq::GenericError newFile(Node&, const char*, const char*);
		ubiq::GenericError removeNode(Node&);

		ubiq::GenericError openStream(Node&, Stream&);
		ubiq::GenericError flushStream(Stream&);
		ubiq::GenericError closeStream(Stream&);

		class Stream {
		private:
			Node *node;
			uint32_t page, offset;
			void *buffer;
			bool written;

			ubiq::GenericError fetchPage();
			ubiq::GenericError access(void* &content, uint32_t size);

			friend WtfsMain;
			inline void initialize(Node *tree);
		public:
			inline Stream();

			enum Whence {
				Start, Current, End
			};

			ubiq::GenericError read(void* &content, uint32_t size);
			ubiq::GenericError write(void* &content, uint32_t size);
			ubiq::GenericError setPosition(Whence whence, int32_t offset);
			ubiq::GenericError flush();

			inline uint32_t getPosition();
			inline uint32_t getSize();
		};
	};
};

#include "MetaImpl.h"
#include "StreamImpl.h"
#include "StorageImpl.h"
#include "GcImpl.h"
#include "MountImpl.h"

template<class Config>
struct WtfsTestHelper {
	using FlashDriver = typename WtfsEcosystem<Config>::FlashDriver;
	using Allocator   = typename WtfsEcosystem<Config>::Allocator;
	using Buffers     = typename WtfsEcosystem<Config>::Buffers;
	using Manager     = typename WtfsEcosystem<Config>::Manager;
	using Node        = typename WtfsEcosystem<Config>::Node;
	using MetaStore   = typename WtfsEcosystem<Config>::MetaStore;
	using BlobStore   = typename WtfsEcosystem<Config>::BlobStore;
	using MetaTree    = typename WtfsEcosystem<Config>::MetaTree;

	template<class Callback>
	static ubiq::GenericError traverseNode(Node &node, Callback &&c) {
		typename WtfsTestHelper<Config>::BlobStore::ReadWriteSession session(&node);
		return node.traverse(session, c);
	}
};


#endif /* WTFSECOSYSTEM_H_ */
