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

#include <cstdint>

#include "BlobTree.h"

template<class Storage, class Allocator, uint32_t predLevelCount>
inline BlobTree<Storage, Allocator, predLevelCount>::BlobTree(Address root, uint32_t size): root(root), size(size) {}

template<class Storage, class Allocator, uint32_t predLevelCount>
inline uint32_t BlobTree<Storage, Allocator, predLevelCount>::getSize() {
	return size;
}

template<class Storage, class Allocator, uint32_t predLevelCount>
inline pet::FailPointer<void> BlobTree<Storage, Allocator, predLevelCount>::empty() {
	RWSession session(this);
	this->upgrade(session);

	void *ret = this->Storage::empty(session, 0);

	if(ret)
		this->commit(session);
	else
		this->closeReadWriteSession(session);

	return ret;
}

template<class Storage, class Allocator, uint32_t predLevelCount>
inline void BlobTree<Storage, Allocator, predLevelCount>::release(void* buffer) {
	ROSession session(this);
	this->Storage::release(session, buffer);
	this->closeReadOnlySession(session);
}

template<class Storage, class Allocator, uint32_t predLevelCount>
pet::FailPointer<void> BlobTree<Storage, Allocator, predLevelCount>::read(uint32_t page)
{
	if((page * Storage::pageSize > size) || (root == Storage::InvalidAddress))
		return 0;

	Address address = root;

	ROSession session(this);

	for(int32_t levels = BlackMagic::getHighestLevel((size - 1) / Storage::pageSize); levels >= 0; levels--) {
		void *ret = this->Storage::read(session, address);

		if(!ret)
			return 0;

		Address *table = (Address *) ret;
		Address newAddress = table[BlackMagic::getLevelOffset(page, levels)];
		this->Storage::release(session, table);
		address = newAddress;
	}

	void *ret = this->Storage::read(session, address);
	this->closeReadOnlySession(session);
	return ret;
}

// NOTE: The local variables representing some level kind of info use an unusual indexing scheme,
//       not consistent with the storage view, that assumes non-negative, increasing level indices.
//       Here the level -1 is the user data level, and level 0 is the lowest index level.
//       This makes more sense, because the user levels are not handled directly by any fs stuff.
//       And it also makes the exponential/logarithmic level size and tree height calculation template
//       magic mechanism simpler, and that's nice because that is quite complicated by nature.

template<class Storage, class Allocator, uint32_t predLevelCount>
pet::GenericError BlobTree<Storage, Allocator, predLevelCount>::update(uint32_t page, uint32_t newSize, void* buffer)
{
	Address writtenAddress;
	RWSession session(this);

	if(newSize < size)
		newSize = size;

	if(!size) {
		if(page != 0) {
			this->Storage::release(session, buffer);
			return pet::GenericError::invalidSeekError();
		}

		this->upgrade(session);
		writtenAddress = this->write(session, buffer);

		if(writtenAddress == Storage::InvalidAddress) {
			this->rollback(session);
			return pet::GenericError::writeError();
		}
	} else {
		const uint32_t oldLastPage = (size - 1) / Storage::pageSize;

		if(page > oldLastPage + 1) {
			this->Storage::release(session, buffer);
			return pet::GenericError::invalidSeekError();
		}

		const int32_t oldLevels = BlackMagic::getHighestLevel(oldLastPage);
		const int32_t newLevels = BlackMagic::getHighestLevel(page);

		if(newLevels > oldLevels) {
			BlobTreeTrace::assert(newLevels == (oldLevels+1));

			this->upgrade(session);
			writtenAddress = this->write(session, buffer);

			if(writtenAddress == Storage::InvalidAddress)
				return pet::GenericError::writeError();

			for(int32_t l = 0; l < newLevels; l++) {
				void *ret = this->Storage::empty(session, l + 1);

				if(!ret) {
					this->rollback(session);
					return pet::GenericError::writeError();
				}

				Address *table = (Address *) ret;

				table[0] = writtenAddress;
				writtenAddress = this->write(session, table);

				if(writtenAddress == Storage::InvalidAddress) {
					this->rollback(session);
					return pet::GenericError::writeError();
				}
			}

			void *ret = this->Storage::empty(session, newLevels + 1);

			if(!ret) {
				this->rollback(session);
				return pet::GenericError::writeError();
			}

			Address *table = (Address *) ret;
			table[0] = root;
			table[1] = writtenAddress;
			writtenAddress = this->write(session, table);

			if(writtenAddress == Storage::InvalidAddress) {
				this->rollback(session);
				return pet::GenericError::writeError();
			}
		} else {
			int32_t level = oldLevels; // see note on top

			pet::DynamicStack<Address, Allocator, predLevelCount> addresses;

			Address address = root;

			while(level >= 0) {
				const uint32_t maxIdx = BlackMagic::getLevelOffset(oldLastPage, level);
				const uint32_t idx = BlackMagic::getLevelOffset(page, level);

				level--;

				if(addresses.acquire().failed()) {
					this->Storage::release(session, buffer);
					this->rollback(session);
					return pet::GenericError::outOfMemoryError();
				}

				*addresses.current() = address;

				if(idx > maxIdx) {
					while(level >= 0) {
						level--;

						if(addresses.acquire().failed()) {
							this->Storage::release(session, buffer);
							this->rollback(session);
							return pet::GenericError::outOfMemoryError();
						}

						*addresses.current() = Storage::InvalidAddress;
					}
					break;
				} else {
					if((level < 0))
						break;

					void *ret = this->Storage::read(session, address);

					if(!ret) {
						this->Storage::release(session, buffer);
						this->rollback(session);
						return pet::GenericError::readError();
					}

					Address *table = (Address *)ret;
					address = table[idx];
					this->Storage::release(session, table);

					if(idx != maxIdx)
						break;
				}
			}

			for(;level >= 0; level--) {
				if(addresses.acquire().failed()) {
					this->Storage::release(session, buffer);
					this->rollback(session);
					return pet::GenericError::outOfMemoryError();
				}

				*addresses.current() = address;

				if(level == 0)
					break;

				void *ret = this->Storage::read(session, address);

				if(!ret) {
					this->Storage::release(session, buffer);
					this->rollback(session);
					return 0;
				}

				Address *table = (Address *)ret;
				address = table[BlackMagic::getLevelOffset(page, level)];
				this->Storage::release(session, table);
			}

			this->upgrade(session);

			writtenAddress = this->write(session, buffer);

			if(writtenAddress == Storage::InvalidAddress) {
				this->rollback(session);
				return pet::GenericError::writeError();
			}

			for(int32_t l=0; addresses.current(); l++) {
				void *ret;

				if(*addresses.current() != Storage::InvalidAddress) {
					ret = (Address *)this->Storage::read(session, *addresses.current());
					if(!ret) {
						this->rollback(session);
						return pet::GenericError::readError();
					}
				} else {
					ret = (Address *)this->Storage::empty(session, l+1);
					if(!ret) {
						this->rollback(session);
						return pet::GenericError::writeError();
					}
				}

				Address *table = (Address *)ret;

				addresses.release();
				table[BlackMagic::getLevelOffset(page, l)] = writtenAddress;
				writtenAddress = this->write(session, table);

				if(writtenAddress == Storage::InvalidAddress) {
					this->rollback(session);
					return pet::GenericError::writeError();
				}
			}

		}
	}

	size = newSize;
	root = writtenAddress;
	this->commit(session);
	return 0;
}

template<class Storage, class Allocator, uint32_t predLevelCount>
template<class ElementCallback>
pet::GenericError BlobTree<Storage, Allocator, predLevelCount>::traverse(RWSession &session, ElementCallback &&callback)
{
	if(!size)
		return 1;

	const uint32_t lastPage = (size - 1) / Storage::pageSize;
	int32_t indexLevel = BlackMagic::getHighestLevel(lastPage); 		// see note on top
	bool update = false;

	Traversor levelStates;

	Address newAddress = Storage::InvalidAddress;

	if(indexLevel == -1) {
		newAddress = callback(root, 0, levelStates);
		update = newAddress != root;
	} else {

		if(levelStates.acquire().failed())
			return pet::GenericError::outOfMemoryError();

		levelStates.current()->idx = 0;
		levelStates.current()->maxIdx = BlackMagic::getLevelOffset(lastPage, indexLevel);
		levelStates.current()->lastEntryOnLevel = true;
		levelStates.current()->address = root;

		while(levelStates.current()) {
			if(indexLevel == 0) {
				void *ret = this->Storage::read(session, levelStates.current()->address);

				if(!ret)
					return pet::GenericError::readError();

				Address *table = (Address *)ret;

				for(levelStates.current()->idx=0; levelStates.current()->idx<=levelStates.current()->maxIdx; levelStates.current()->idx++) {
					newAddress = callback(table[levelStates.current()->idx], 0, levelStates);
					if(newAddress != table[levelStates.current()->idx]) {
						this->Storage::release(session, table);
						update = true;
						levelStates.current()->idx++;
						break;
					}
				}

				if(update)
					break;

				indexLevel = 1;

				this->Storage::release(session, table);

				Address addr = levelStates.current()->address;
				levelStates.release();

				newAddress = callback(addr, 1, levelStates);
				if(newAddress != addr) {
					update = true;
					break;
				}
			} else {
				void *ret = this->Storage::read(session, levelStates.current()->address);

				if(!ret)
					return pet::GenericError::readError();

				Address *table = (Address *)ret;

				if(levelStates.current()->idx <= levelStates.current()->maxIdx) {
					Address childAddress = table[levelStates.current()->idx];
					this->Storage::release(session, table);

					State *parent = levelStates.current();

					if(levelStates.acquire().failed())
						return pet::GenericError::outOfMemoryError();

					indexLevel--;

					if(parent->lastEntryOnLevel && parent->idx == parent->maxIdx) {
						levelStates.current()->lastEntryOnLevel = true;
						levelStates.current()->maxIdx = BlackMagic::getLevelOffset(lastPage, indexLevel);
					} else {
						levelStates.current()->lastEntryOnLevel = false;
						levelStates.current()->maxIdx = BlackMagic::base - 1;
					}

					levelStates.current()->idx = 0;
					levelStates.current()->address = childAddress;

					parent->idx++;
				} else {
					this->Storage::release(session, table);
					indexLevel++;
					Address addr = levelStates.current()->address;
					levelStates.release();

					newAddress = callback(addr, indexLevel, levelStates);
					if(newAddress != addr) {
						update = true;
						break;
					}
				}
			}
		}
	}

	if(newAddress != Storage::InvalidAddress && update) {
		while(levelStates.current()) {
			void *rret = this->Storage::read(session, levelStates.current()->address);

			if(!rret)
				return pet::GenericError::readError();

			Address *table = (Address *)rret;

			table[levelStates.current()->idx - 1] = newAddress;

			auto wret = this->Storage::write(session, table);

			if(wret == Storage::InvalidAddress)
				return pet::GenericError::writeError();

			newAddress = wret;
			levelStates.release();
		};

		root = newAddress;
		return true;
	}

	return false;
}

template <class Storage, class Allocator, uint32_t predLevelCount>
pet::GenericError BlobTree<Storage, Allocator, predLevelCount>::dispose()
{
	RWSession session(this);
	this->upgrade(session);

	pet::GenericError ret = traverse(session, [&](typename Storage::Address addr, uint32_t level, const Traversor&) -> typename Storage::Address {
		this->disposeAddress(session, addr);
		return addr;
	});

	if(ret.failed()) {
		this->rollback(session);
		return ret.rethrow();
	}

	root = Storage::InvalidAddress;
	size = 0;

	this->commit(session);

	return ret;
}

template<class Storage, class Allocator, uint32_t predLevelCount>
inline pet::GenericError BlobTree<Storage, Allocator, predLevelCount>::relocate(Address &page)
{
	RWSession session(this);
	this->upgrade(session);

	pet::GenericError res = this->traverse(session, [&](Address addr, uint32_t level, const Traversor&) -> Address {
		if(addr == page) {
			void* ret = this->Storage::read(session, addr);

			if(!ret)
				return Storage::InvalidAddress;

			return page = this->Storage::write(session, ret);
		}

		return addr;
	});

	if(res.failed()) {
		this->rollback(session);
		return res.rethrow();
	} else if(res) {
		this->commit(session);
		return true;
	} else {
		this->closeReadWriteSession(session);
		return false;
	}
}
