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

#ifndef _LOCATOR_H

#include <cstdint>

#include "BTree.h"

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
BTree<Storage, Key, IndexKey, Value, Allocator>
::Iterator::Iterator(const IndexKey& k) : key(k) {}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline bool BTree<Storage, Key, IndexKey, Value, Allocator>
::Iterator::hasNext() const
{
	for(auto it = locator.iterator(); it.current(); it.step())
		if (it.current()->hasMore())
			return true;

	return false;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline void BTree<Storage, Key, IndexKey, Value, Allocator>
::Iterator::updateSiblings(Node* index){
	if(locator.current()->idx > 0) {
		locator.current()->smallerSibling = index->children[locator.current()->idx-1];
		locator.current()->smallerValue = index->values[locator.current()->idx-1];
	} else
		locator.current()->smallerSibling = InvalidAddress;

	if(locator.current()->idx < index->numBranches-1) {
		locator.current()->greaterSibling = index->children[locator.current()->idx+1];
		locator.current()->greaterValue = index->values[locator.current()->idx];
	} else
		locator.current()->greaterSibling = InvalidAddress;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template <class Comparator>
inline pet::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::stepDown(ROSession &session, Iterator& iterator, Address address, uint32_t level) {
	BtreeTrace::assert(level > 0);
	int32_t rootNum = 0;
	do {
		void *ret = (Node*) this->read(session, address);

		if(!ret)
			return pet::GenericError::readError();

		Node* index = (Node*) ret;

		if(!rootNum)
			rootNum = index->length();

		pet::Bisect::Result pos = pet::Bisect::
				find<IndexKey, IndexKey, Comparator>(index->values, index->numBranches - 1, iterator.key);

		if(iterator.locator.acquire().failed()) {
			this->release(session, index);
			return pet::GenericError::outOfMemoryError();
		}

		iterator.locator.current()->address = address;

		if (pos.present()) {
			iterator.locator.current()->idx = pos.first();
			iterator.locator.current()->max = pos.last() + 1;
		} else {
			iterator.locator.current()->idx = pos.insertionIndex();
			iterator.locator.current()->max = pos.insertionIndex();
		}

		iterator.updateSiblings(index);
		if (level == 1)
			iterator.currentAddress = index->children[iterator.locator.current()->idx];
		else
			address = index->children[iterator.locator.current()->idx];

		this->release(session, index);
	} while (--level);

	return rootNum == 2;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template <class Comparator>
inline pet::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::step(ROSession &session, Iterator& iterator)
 {
	if (iterator.locator.current()->hasMore()) {
		void *ret = this->read(session, iterator.locator.current()->address);

		if(!ret)
			return pet::GenericError::readError();

		Node* index = (Node*) ret;

		iterator.locator.current()->idx++;
		iterator.updateSiblings(index);
		iterator.currentAddress = index->children[iterator.locator.current()->idx];

		this->release(session, index);
	} else {
		uint32_t nLevels = 0;
		while (iterator.locator.hasMore()) {
			Address lastAddress = iterator.locator.current()->address;
			bool ok = iterator.locator.release();
			BtreeTrace::assert(ok);
			nLevels++;
			if (iterator.locator.current()->hasMore()) {
				void *ret = this->read(session, iterator.locator.current()->address);

				if(!ret)
					return pet::GenericError::readError();

				Node* index = (Node*) ret;

				iterator.locator.current()->idx++;
				Address nextAddress = index->children[iterator.locator.current()->idx];
				iterator.updateSiblings(index);

				this->release(session, index);

				return stepDown<Comparator>(session, iterator, nextAddress, nLevels);
			}
		};

		iterator.currentAddress = InvalidAddress;
	}

	return true;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template <class Comparator>
inline pet::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::iterate(ROSession &session, Iterator& iterator)
 {
	while(iterator.locator.release()) {}
	return this->stepDown<Comparator>(session, iterator, root, levels);
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress
BTree<Storage, Key, IndexKey, Value, Allocator>
::doUpdate(RWSession &session, Locator& locator, Address updatedAddress)
{
	void *ret = this->read(session, locator.current()->address);

	if(!ret)
		return InvalidAddress;

	Node* index = (Node*) ret;
	index->children[locator.current()->idx] = updatedAddress;

	return this->Storage::write(session, index);
}


template <class Storage, class Key, class IndexKey, class Value, class Allocator>
typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress
BTree<Storage, Key, IndexKey, Value, Allocator>
::propagateUpdate(RWSession &session, Locator& locator, Address updatedAddress)
{
	while (locator.release()) {
		if(!locator.hasMore())
			this->flagNextAsRoot(session);
		FailAddress ret = doUpdate(session, locator, updatedAddress);

		if(ret.failed())
			return ret.rethrow();

		updatedAddress = ret;
	}

	return updatedAddress;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress
BTree<Storage, Key, IndexKey, Value, Allocator>
::updateOne(RWSession &session, Locator& locator, Address updatedAddress)
{
	if(!locator.hasMore())
		this->flagNextAsRoot(session);

	FailAddress ret = doUpdate(session, locator, updatedAddress);

	if(ret.failed())
		return ret.rethrow();

	return propagateUpdate(session, locator, ret);
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress
BTree<Storage, Key, IndexKey, Value, Allocator>
::updateTwo(RWSession &session, Locator& locator, UpdateDirection dir, IndexKey newHash, Address updatedAddress, Address otherAddress)
{
	void *ret = this->read(session, locator.current()->address);

	if(!ret)
		return InvalidAddress;

	Node* index = (Node*) ret;
	index->children[locator.current()->idx] = updatedAddress;
	if(dir == UpdateDirection::Up){
		BtreeTrace::assert(locator.current()->idx < index->numBranches - 1);
		index->children[locator.current()->idx+1] = otherAddress;
		index->values[locator.current()->idx] = newHash;
	} else {
		BtreeTrace::assert(locator.current()->idx > 0);
		index->children[locator.current()->idx-1] = otherAddress;
		index->values[locator.current()->idx-1] = newHash;
	}

	if(!locator.hasMore())
		this->flagNextAsRoot(session);

	Address newAddress = this->Storage::write(session, index);
	if(newAddress == Storage::InvalidAddress)
		return Storage::InvalidAddress;

	return propagateUpdate(session, locator, newAddress);
}


#endif
