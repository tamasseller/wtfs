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

#ifndef STORAGE_H_
#define STORAGE_H_

#include <cstdint>

#include "Wtfs.h"

#include "pool/Fifo.h"

#include "meta/MemberPointers.h"

template<class BackendConfig, class Allocator, class Child>
class StorageBase {
	void checkGc();
public:
	typedef typename BackendConfig::Address Address;
	static constexpr Address InvalidAddress = BackendConfig::InvalidAddress;

	struct PageMeta {
		union {
			struct {
				uint32_t id;
				uint32_t parentId;
			};

			struct {
				uint32_t sequenceNumber;
			};
		};
	};

	class ReadOnlySession {
	protected:
		inline ReadOnlySession() {}
	public:
		inline ReadOnlySession(StorageBase* self);
	};

	inline void *read(ReadOnlySession& session, Address p);
	inline void release(ReadOnlySession& session, void* p);
	inline void closeReadOnlySession(ReadOnlySession& session);

	class ReadWriteSession: public ReadOnlySession {
		friend StorageBase;
		mm::DynamicFifo<Address, Allocator, 4> garbage, newish;
	public:
		inline ReadWriteSession(StorageBase* self);
	};

	inline void upgrade(ReadWriteSession& session);
	inline void *empty(ReadWriteSession& session, int32_t unused);
	inline Address write(ReadWriteSession& session, void* p);
	inline void disposeBuffered(ReadWriteSession& session, void* p);
	inline void disposeAddress(ReadWriteSession& session, Address p);
	inline void rollback(ReadWriteSession& session);
	inline void commit(ReadWriteSession& session);
	inline void closeReadWriteSession(ReadWriteSession& session);
};

template<class BackendConfig, class Allocator, class Fs, class FsBuffers>
struct MetaStorage: public StorageBase<BackendConfig, Allocator, MetaStorage<BackendConfig, Allocator, Fs, FsBuffers>> {
	typedef StorageBase<BackendConfig, Allocator, MetaStorage> Base;
	typedef FsBuffers Buffers;

	static const uint32_t pageSize = FsBuffers::pageSize - sizeof(typename Base::PageMeta);

	struct Page {
		uint8_t payload[pageSize];
		typename Base::PageMeta meta;
	};

	static_assert(sizeof(Page) == FsBuffers::pageSize, "Wrong page size (possibly int32_ternal error)");

	class ReadWriteSession: public Base::ReadWriteSession {
		friend MetaStorage;
		bool addSeqNumber = false;
	public:
		inline ReadWriteSession(Base* self): Base::ReadWriteSession(self) {}
	};

	inline typename Base::Address write(ReadWriteSession& session, void* p)
	{
		((Page*) p)->meta.sequenceNumber = (session.addSeqNumber) ? ((Fs*)this)->updateCounter : 0;

		typename Base::Address ret = Base::write(session, p);

		if(session.addSeqNumber && ret != Base::InvalidAddress)
			((Fs*)this)->updateCounter++;

		return ret;
	}

	inline void flagNextAsRoot(ReadWriteSession& session) {
		session.addSeqNumber = true;
	}

private:
	friend Base;

	static inline Fs& getFs(Base *self) {
		return *((Fs*)self);
	}
};

template<class BackendConfig, class Allocator, class Fs, class FsBuffers, class Node>
struct BlobStorage: public StorageBase<BackendConfig, Allocator, BlobStorage<BackendConfig, Allocator, Fs, FsBuffers, Node>> {
	typedef StorageBase<BackendConfig, Allocator, BlobStorage> Base;
	typedef FsBuffers Buffers;

	static const uint32_t pageSize = FsBuffers::pageSize - sizeof(typename Base::PageMeta);

	struct Page {
		uint8_t payload[pageSize];
		typename Base::PageMeta meta;
	};

	static_assert(sizeof(Page) == FsBuffers::pageSize, "Wrong page size (possibly int32_ternal error)");

	inline void *empty(typename Base::ReadWriteSession& session, int32_t level) {
		void * ret = Base::empty(session, -1 - level);
		((Page*) ret)->meta.id = ((Node*)this)->key.id;
		((Page*) ret)->meta.parentId = ((Node*)this)->key.indexed.parentId;
		return ret;
	}
private:
	friend Base;

	static inline Fs& getFs(Base *self) {
		return *(((Node*)self)->fs);
	}
};


#endif /* STORAGE_H_ */
