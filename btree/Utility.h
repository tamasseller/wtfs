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

#ifndef UTILITY_H_
#define UTILITY_H_

#include <cstdint>

#include "BTree.h"
#include "pool/Stack.h"

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template<class ElementCallback>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::traverse(RWSession &session, ElementCallback&& callback)
{
	Traversor stack;
	bool update = false;
	Address currAddress = root;
	Address newAddress = Storage::InvalidAddress;

	if(!levels) {
		if(root != InvalidAddress) {
			newAddress = callback(root, 0, stack);
			update = newAddress != root;
		} else
			return false;
	} else {
		int32_t currLevel = levels;

		while(1) {
			if(stack.acquire().failed())
				return ubiq::GenericError::outOfMemoryError();

			*stack.current() = currAddress;

			if(currLevel == 1)
				break;

			void* ret = this->read(session, currAddress);

			if(!ret)
				return ubiq::GenericError::readError();

			currAddress = ((Node*)ret)->children[0];

			this->release(session, ret);
			currLevel--;
		}

		while(stack.current()) {
			if(currLevel == 1) {
				currAddress = *stack.current();
				void* ret = this->read(session, currAddress);

				if(!ret)
					return ubiq::GenericError::readError();

				Node *node = (Node*)ret;
				for(uint32_t i = 0; i < node->length(); i++) {
					newAddress = callback(node->children[i], 0, stack);
					update = newAddress != node->children[i];
					if(update) {
						this->release(session, node);
						currAddress = node->children[i];
						break;
					}
				}

				if(update)
					break;

				this->release(session, node);
				stack.release();

				newAddress = callback(currAddress, 1, stack);
				update = newAddress != currAddress;
				if(update)
					break;

				currLevel++;
			} else {
				void* ret = this->read(session, *stack.current());

				if(!ret)
					return ubiq::GenericError::readError();

				Node *node = (Node*)ret;

				uint32_t childIndex = -1u;
				for(uint32_t i=0; i<node->length(); i++) {
					if(node->children[i] == currAddress) {
						childIndex = i;
						break;
					}
				}

				currAddress = *stack.current();

				if(childIndex == node->length() - 1) { // coming back from last child, go up
					this->release(session, node);
					stack.release();

					newAddress = callback(currAddress, currLevel, stack);
					update = newAddress != currAddress;
					if(update)
						break;

					currLevel++;
				} else {
					// If coming from int32_ternal node below, go for the next.
					// If coming from top, then not found so childIndex is -1 => it selects the first child.
					Address nextChild = node->children[childIndex + 1];
					this->release(session, node);

					if(stack.acquire().failed())
						return ubiq::GenericError::outOfMemoryError();

					*stack.current() = nextChild;
					currLevel--;
				}
			}
		}
	}

	if(newAddress != Storage::InvalidAddress && update) {
		while(stack.current()) {
			void *rret = this->Storage::read(session, *stack.current());

			if(!rret)
				return ubiq::GenericError::readError();

			Node *node = (Node *)rret;

			bool ok = false;
			for(uint32_t i=0; i<node->length(); i++) {
				if(node->children[i] == currAddress) {
					node->children[i] = newAddress;
					ok = true;
					break;
				}
			}

			BtreeTrace::assert(ok, "Internal error (node not found during traverse update)");

			if(!stack.hasMore())
				this->Storage::flagNextAsRoot(session);

			auto wret = this->Storage::write(session, (void*)node);

			if(wret == Storage::InvalidAddress)
				return ubiq::GenericError::writeError();

			newAddress = wret;
			currAddress = *stack.current();
			stack.release();
		}

		root = newAddress;
		return true;
	}

	return false;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>::purge()
{
	RWSession rwSession(this);
	this->upgrade(rwSession);

	ubiq::GenericError ret = traverse(rwSession, [&](Address addr, uint32_t level, const Traversor &parents) -> Address {
		this->disposeAddress(rwSession, addr);
		return addr;
	});

	if(ret.failed()) {
		this->rollback(rwSession);
		return ret.rethrow();
	}

	root = InvalidAddress;
	levels = 0;

	this->commit(rwSession);

	return ret;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline ubiq::GenericError
BTree<Storage, Key, IndexKey, Value, Allocator>::relocate(Address &page)
{
	RWSession session(this);
	this->upgrade(session);

	ubiq::GenericError res = this->traverse(session, [&](Address addr, uint32_t level, const Traversor&) -> Address {
		if(addr == page) {
			void* ret = this->read(session, addr);

			if(!ret)
				return Storage::InvalidAddress;

			if(page == this->root)
				this->flagNextAsRoot(session);
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

#endif /* UTILITY_H_ */
