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

#ifndef STORAGEMANAGER_H_
#define STORAGEMANAGER_H_

#include <cstdint>

#include "ubiquitous/Error.h"
#include "ubiquitous/Trace.h"

class StorageManagerTrace;

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
class StorageManager: pet::Trace<StorageManagerTrace> {
	public:
		typedef typename FlashDriver::Address Address;
	protected:
		static constexpr uint32_t maxLevels = maxMetaLevels + maxFileLevels;

		uint8_t usageCounters[FlashDriver::deviceSize];

		struct AllocationState {
			uint32_t currentAddress = -1u;
			uint32_t usedCount = -1u;
		};

		AllocationState levelAllocations[maxLevels];

		inline Address findFree();

		constexpr static inline uint32_t levelToIndex(int32_t level);
		constexpr static inline int32_t indexToLevel(uint32_t index);
		constexpr static inline bool indexOk(uint32_t idx);

		inline bool updateLevel(uint32_t level);
		uint32_t spareCount;
	public:
		inline bool initWithDefaultAssignment();

		inline bool updateAllocationState();

		inline Address allocate(int32_t level);
		inline void claim(Address addr);
		inline void reclaim(Address addr);
		inline bool isBlockBeingUsed(uint32_t);

		inline bool gcNeeded() {return spareCount <= maxLevels;}

		class Iterator {
			int32_t index;

			static inline int32_t forwardSearch(StorageManager &, int32_t);
			static inline int32_t backwardSearch(StorageManager &, int32_t, int32_t);
		public:
			inline Iterator(StorageManager &);
			inline int32_t currentCount(StorageManager &);
			inline int32_t currentBlock();
			inline void step(StorageManager &);
		};
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
constexpr inline uint32_t
StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::
levelToIndex(int32_t level)
{
	return 	(level < 0) ? -level-1 : (level + (int32_t)maxFileLevels);
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
constexpr inline bool
StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::
indexOk(uint32_t idx)
{
	return idx < maxLevels;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
constexpr inline int32_t
StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::
indexToLevel(uint32_t index)
{
	return (index < maxFileLevels) ? -(int32_t)index-1 : ((int32_t)index - (int32_t)maxFileLevels);
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline bool StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::initWithDefaultAssignment()
{
	spareCount = FlashDriver::deviceSize;

	for(uint32_t i=0; i<FlashDriver::deviceSize; i++)
		this->usageCounters[i] = 0;

	for(uint32_t i=0; i<maxLevels; i++) {
		this->levelAllocations[i].currentAddress = findFree();

		if(this->levelAllocations[i].currentAddress == -1u)
			return false;

		this->levelAllocations[i].usedCount = 0;
	}

	return true;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline bool StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::isBlockBeingUsed(uint32_t block)
{
	for(uint32_t i=0; i<maxLevels; i++) {
		if(this->levelAllocations[i].currentAddress == block)
			return true;
	}

	return false;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
typename StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Address
inline StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::findFree()
{
	for(uint32_t i=0; i<FlashDriver::deviceSize; i++) {
		if(!this->usageCounters[i]) {
			this->usageCounters[i] = FlashDriver::blockSize;
			FlashDriver::ensureErased(i);
			spareCount--;
			return i;
		}
	}

	return -1u;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline void StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::reclaim(Address addr) {
	uint32_t blockAddress = addr / FlashDriver::blockSize;
	this->usageCounters[blockAddress]--;

	info << "address "<< addr << " reclaimed";

	if(!this->usageCounters[blockAddress]) {
		spareCount++;
		info << " and block " << blockAddress << " is now free\n";
	}else
		info << "\n";
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline void StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::claim(Address addr) {
	uint32_t blockAddress = addr / FlashDriver::blockSize;
	this->usageCounters[blockAddress]++;
	info << "address "<< addr << " claimed\n";
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
typename StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Address
inline StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::allocate(int32_t level)
{
	if((level >= (int32_t)maxMetaLevels) || (level < -(int32_t)maxFileLevels))
		return FlashDriver::InvalidAddress;

	const uint32_t index = levelToIndex(level);

	if(levelAllocations[index].usedCount == FlashDriver::blockSize) {
		uint32_t newEmpty = findFree();

		if(newEmpty == -1u) {
			warn << "allocation FAILED for level " << level << "\n";
			return FlashDriver::InvalidAddress;
		}

		levelAllocations[index].usedCount = 0;
		levelAllocations[index].currentAddress = newEmpty;
		info << "block " << newEmpty << " allocated for level " << level << "\n";
	}

	Address ret = levelAllocations[index].currentAddress * FlashDriver::blockSize + levelAllocations[index].usedCount++;

	info << "page " << ret << " allocated for level " << level << " request\n";

	return ret;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline int32_t StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Iterator::
forwardSearch(StorageManager &manager, int32_t min)
{
	int32_t ret = -1;
	int32_t act = 0xff;

	for(uint32_t i=0; i<FlashDriver::deviceSize; i++) {
		if((int32_t)manager.usageCounters[i] > min && (int32_t)manager.usageCounters[i] <= act) {
			ret = i;
			act = (int32_t)manager.usageCounters[i];
		}
	}

	return ret;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline int32_t StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Iterator::
backwardSearch(StorageManager &manager, int32_t value, int32_t place)
{
	while(place){
		--place;

		if(manager.usageCounters[place] == value)
			return place;
	}

	return -1;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Iterator::Iterator(StorageManager &manager)
{
	index = forwardSearch(manager, 0);
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline int32_t StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Iterator::currentCount(StorageManager &manager)
{
	if(index >= 0)
		return manager.usageCounters[index];

	return -1;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline int32_t StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Iterator::currentBlock()
{
	return index;
}

template <class FlashDriver, uint32_t maxMetaLevels, uint32_t maxFileLevels>
inline void StorageManager<FlashDriver, maxMetaLevels, maxFileLevels>::Iterator::step(StorageManager &manager)
{
	if(index == -1)
		return;

	int32_t next;
	next = backwardSearch(manager, manager.usageCounters[index], index);

	if(next != -1)
		index = next;
	else
		index = forwardSearch(manager, manager.usageCounters[index]);
}

#endif /* STORAGEMANAGER_H_ */
