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

#ifndef _TABLE_H

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline void BTree<Storage, Key, IndexKey, Value, Allocator>::Element::set(const Key &k, const Value &v) {
	key = k;
	value = v;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline bool BTree<Storage, Key, IndexKey, Value, Allocator>::Element::operator >(const Key& than) const {
	return key > than;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline bool BTree<Storage, Key, IndexKey, Value, Allocator>::Element::operator ==(const Key& to) const {
	return key == to;
}


template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline uint32_t BTree<Storage, Key, IndexKey, Value, Allocator>::Table::length() const {
	uint32_t ret = 0;

	while (ret < maxElements) {
		if (elements[ret] == InvalidKey)
			break;

		ret++;
	}

	return ret;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline void BTree<Storage, Key, IndexKey, Value, Allocator>::Table::terminate(const uint32_t idx) {
	if (idx < maxElements)
		elements[idx].key = InvalidKey;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline typename BTree<Storage, Key, IndexKey, Value, Allocator>::Element &BTree<Storage, Key, IndexKey, Value, Allocator>::Table::makeRoom(uint32_t insIdx, uint32_t nElements) {
	for (uint32_t i = nElements; i > insIdx; i--)
		elements[i] = elements[i - 1];

	return elements[insIdx];
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline void BTree<Storage, Key, IndexKey, Value, Allocator>::Table::remove(uint32_t insIdx, uint32_t nElements) {
	for (uint32_t i = insIdx; i < nElements - 1; i++)
		elements[i] = elements[i + 1];

	terminate(nElements - 1);
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline pet::FailPointer<typename BTree<Storage, Key, IndexKey, Value, Allocator>::Table>
BTree<Storage, Key, IndexKey, Value, Allocator>::
splitTable(RWSession& session, Table &table, uint32_t insIdx, const Key& key, Value address)
{
	void *ret = this->empty(session, 0);
	if(!ret)
		return 0;

	constexpr auto maxElements = Table::maxElements;
	constexpr auto splitPoint32_t = Table::splitPoint32_t;

	Table *newTable = (Table*) ret;

	if (insIdx < splitPoint32_t) {
		for (uint32_t i = 0; i < (maxElements - splitPoint32_t + 1); i++)
			newTable->elements[i] = table.elements[i + splitPoint32_t - 1];

		newTable->terminate(maxElements - splitPoint32_t + 1);

		table.makeRoom(insIdx, splitPoint32_t).set(key, address);
		table.terminate(splitPoint32_t);
	} else {
		insIdx -= splitPoint32_t;
		for (uint32_t i = 0; i < insIdx; i++)
			newTable->elements[i] = table.elements[i + splitPoint32_t];

		newTable->elements[insIdx].set(key, address);

		for (uint32_t i = insIdx; i < (maxElements - splitPoint32_t); i++)
			newTable->elements[i + 1] = table.elements[i + splitPoint32_t];

		newTable->terminate(maxElements - splitPoint32_t + 1);
		table.terminate(splitPoint32_t);
	}

	return newTable;
}

#endif
