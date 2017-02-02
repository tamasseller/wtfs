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

#ifndef _NODE_H

#include <cstdint>

#include "BTree.h"

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline void BTree<Storage, Key, IndexKey, Value, Allocator>::Node::insert(uint32_t insIdx, IndexKey hash, Address updatedAddress, Address newAddress)
{
	for (uint32_t i = numBranches - 1; i > insIdx; i--)
		values[i] = values[i - 1];

	for (uint32_t i = numBranches; i > insIdx + 1; i--)
		children[i] = children[i - 1];

	values[insIdx] = hash;
	children[insIdx] = updatedAddress;
	children[insIdx + 1] = newAddress;
	numBranches++;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline void BTree<Storage, Key, IndexKey, Value, Allocator>::Node::remove(uint32_t delIdx, Address newAddress)
{
	for(uint32_t i=delIdx; i<numBranches-2; i++)
		values[i] = values[i+1];

	children[delIdx] = newAddress;

	for(uint32_t i=delIdx+1; i<numBranches-1; i++)
		children[i] = children[i+1];

	numBranches--;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress BTree<Storage, Key, IndexKey, Value, Allocator>
::createRootNode(RWSession& session, IndexKey hash, Address lower, Address higher, int32_t levels)
{
	void* ret = this->empty(session, levels+1);

	if(!ret)
		return InvalidAddress;

	Node *node = (Node *)ret;

	node->numBranches = 2;
	node->values[0] = hash;
	node->children[0] = lower;
	node->children[1] = higher;

	this->flagNextAsRoot(session);
	return this->Storage::write(session, node);
}

#endif
