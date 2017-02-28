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

#ifndef BUFFEREDSTORAGE_H_
#define BUFFEREDSTORAGE_H_

#include <cstdint>

#include "ubiquitous/Error.h"
#include "ubiquitous/Trace.h"

enum BufferReleaseCondition {
		Dirty, Clean, Purge
};

class BufferedStorageTrace: public pet::Trace<BufferedStorageTrace> {};

template <class FlashDriver, class StorageManager, class Mutex, uint32_t nBuffers>
class BufferedStorage: BufferedStorageTrace {
public:
	typedef typename FlashDriver::Address Address;

	class Initializer {
	protected:
		static void initialize(BufferedStorage* storage, StorageManager* manager) {
			storage->storageManager = manager;
		}
	};
private:
	friend Initializer;
	struct ManagementData {
		Address address = FlashDriver::InvalidAddress;
		uint32_t accessCounter = 0, usageCounter = 0;
		bool dirty = false;

		inline ManagementData(): address(FlashDriver::InvalidAddress) {}
	};

public:
	struct StoredData {
		int8_t user[FlashDriver::pageSize - sizeof(uint32_t)];
		uint32_t level;
	};

	static const uint32_t pageSize = sizeof(StoredData::user);


	struct Buffer
	{
		StoredData data;
		ManagementData management;
	};

private:
	uint32_t accessCounter=0;
	Buffer buffers[nBuffers];
	StorageManager *storageManager = 0;
	Mutex mutex;
public:
	void flush();
	Buffer* find(Address addr);
	Address release(Buffer* buff, BufferReleaseCondition cond);
	Address getAddress(Buffer* buff);
};

////////////////////////////////////////////////////////////////////////////////////////

template <class FlashDriver, class StorageManager, class Mutex, uint32_t nBuffers>
void BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::flush() {
	mutex.lock();
	while(1) {
		uint32_t leastRecentCount = 0;
		Buffer* leastRecent = 0;

		for(uint32_t i=0; i<nBuffers; i++) {
			uint32_t unrecentness = accessCounter - buffers[i].management.accessCounter;
			if(buffers[i].management.dirty && unrecentness >= leastRecentCount) {
				leastRecentCount = unrecentness;
				leastRecent = &buffers[i];
			}
		}

		if(!leastRecent)
			break;

		FlashDriver::write(leastRecent->management.address, &leastRecent->data);
		leastRecent->management.dirty = false;
	}
	mutex.unlock();
}

template <class FlashDriver, class StorageManager, class Mutex, uint32_t nBuffers>
typename BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::Buffer*
BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::
find(Address addr)
{
	uint32_t leastRecentDirtyCount = 0, leastRecentCleanCount = 0;
	Buffer *leastRecentDirty = 0, *leastRecentClean = 0;
	Buffer *ret = 0;

	info << "looking for ";
	if(addr == FlashDriver::InvalidAddress)
		info << "empty buffer: ";
	else
		info << "page " << addr << ": ";

	mutex.lock();
	for(uint32_t i=0; i<nBuffers; i++) {
		if(	buffers[i].management.address == addr && 	// Hit
			(addr != FlashDriver::InvalidAddress ||		// If looking for empty it should be free
			buffers[i].management.usageCounter == 0)) {
				ret = &buffers[i];
				info << "found buffer #" << i << "\n";
				break;
		}

		if(buffers[i].management.usageCounter == 0) {	// Eviction candidate search
			uint32_t unrecentness = accessCounter - buffers[i].management.accessCounter;

			if(buffers[i].management.dirty) {
				if(unrecentness >= leastRecentDirtyCount) {
					leastRecentDirtyCount = unrecentness;
					leastRecentDirty = &buffers[i];
				}
			} else {
				// Clean, free, empty ones, should definitely be used first
				if(buffers[i].management.address == FlashDriver::InvalidAddress)
					unrecentness = -1u;

				if(unrecentness >= leastRecentCleanCount) {
					leastRecentCleanCount = unrecentness;
					leastRecentClean = &buffers[i];
				}
			}
		}
	}

	if(!ret) {
		info << "not found, ";

		bool useClean = leastRecentClean != 0;
		if(leastRecentDirty != 0 && leastRecentCleanCount * 2 < leastRecentDirtyCount)
			useClean = false;

		if(useClean) {
			info << "evicting clean buffer " << leastRecentClean-buffers << "\n";
			ret = leastRecentClean;
		} else {
			if(!leastRecentDirty) {
				warn << "buffer request can not be satisfied!\n";
				mutex.unlock();
				return 0;
			}

			info << "flushing dirty buffer " << leastRecentDirty-buffers << "\n";

			FlashDriver::write(leastRecentDirty->management.address, &leastRecentDirty->data);
			leastRecentDirty->management.dirty = false;
			ret = leastRecentDirty;
		}

		ret->management.address = addr;
		if(addr != FlashDriver::InvalidAddress)
			FlashDriver::read(addr, &ret->data);
	}

	ret->management.accessCounter = accessCounter++;
	ret->management.usageCounter++;

	mutex.unlock();
	return ret;
}

template <class FlashDriver, class StorageManager, class Mutex, uint32_t nBuffers>
typename BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::Address
BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::
release(Buffer* buff, BufferReleaseCondition cond)
{
	typename FlashDriver::Address oldAddress = buff->management.address;

	info << "releasing buffer #" << buff-buffers << " (containing: ";

	if(oldAddress == FlashDriver::InvalidAddress)
		info << "fresh data";
	else
		info << "page " << oldAddress;

	info << ") as ";

	mutex.lock();
	if(cond == Purge) {
		info << "garbage (it was " << (buff->management.dirty ? "dirty" : "clean") << ")\n";
		this->storageManager->reclaim(buff->management.address);
		buff->management.address = FlashDriver::InvalidAddress;
		buff->management.dirty = false;
	} else if(cond == Dirty) {
		info << "dirty (it was " << (buff->management.dirty ? "dirty" : "clean") << ")\n";
		if(!buff->management.dirty) {
			buff->management.address = this->storageManager->allocate(buff->data.level);

			// Wipe stale copies
			for(uint32_t i=0; i<nBuffers; i++) {
				if((buffers + i != buff) &&
					(buffers[i].management.address == buff->management.address)) {
					assert(!buffers[i].management.usageCounter, "Wiping occupied page.");
					assert(!buffers[i].management.dirty, "Wiping dirty page (probable write collision).");
					buffers[i].management.address = FlashDriver::InvalidAddress;
				}
			}

			buff->management.dirty = true;

			if(oldAddress != FlashDriver::InvalidAddress)
				this->storageManager->reclaim(oldAddress);
		}
	} else {
		info << "clean (it was " << (buff->management.dirty ? "dirty" : "clean") << ")\n";
	}

	assert(buff->management.usageCounter);
	buff->management.usageCounter--;

	mutex.unlock();
	return buff->management.address;
}

template <class FlashDriver, class StorageManager, class Mutex, uint32_t nBuffers>
typename BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::Address
BufferedStorage<FlashDriver, StorageManager, Mutex, nBuffers>::
getAddress(Buffer* buff) {
	return buff->management.address;
}

#endif /* BUFFEREDSTORAGE_H_ */
