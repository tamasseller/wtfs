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

#ifndef BTREE_H_
#define BTREE_H_

#include <cstdint>

#include "pool/Stack.h"
#include "ubiquitous/Error.h"

#include "algorithm/Find.h"

#include "ubiquitous/Trace.h"

class BtreeTrace: public ubiq::Trace<BtreeTrace> {};

#ifndef BTREE_LOCATOR_LEVELS
#define BTREE_LOCATOR_LEVELS 4
#endif

#ifndef BTREE_TRAVERSOR_LEVELS
#define BTREE_TRAVERSOR_LEVELS BTREE_LOCATOR_LEVELS
#endif

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
class BTree: public Storage {
	friend class BTreeTestUtils;
public:
	struct Element {
		Key key;
		Value value;

		inline void set(const Key &key, const Value &value);
		inline bool operator >(const Key& than) const;
		inline bool operator ==(const Key& to) const;
	};

	typedef algorithm::Bisect::DefaultComparator<IndexKey, IndexKey> FullComparator;

protected:
    //
	// External type aliases
	//

	typedef typename Storage::Address Address;
	typedef typename Storage::ReadWriteSession RWSession;
	typedef typename Storage::ReadOnlySession ROSession;

	constexpr static const Address InvalidAddress = Storage::InvalidAddress;
	constexpr static const Key &InvalidKey = Key::InvalidKey;

	typedef ubiq::FailValue<Address, InvalidAddress> FailAddress;

	//
	// Internal types
	//

	struct Table {
		friend BTree;
		static const uint32_t maxElements = Storage::pageSize/sizeof(Element);
		static_assert(maxElements >= 3, "Page size too small, at least 3 elements needed");
		static const uint32_t splitPoint32_t = (maxElements + 1) / 2;

		Element elements[maxElements];

		inline uint32_t length() const;
		inline void terminate(const uint32_t idx);
		inline Element &makeRoom(uint32_t insIdx, uint32_t nElements);
		inline void remove(uint32_t insIdx, uint32_t nElements);
	};

	struct Node {
		static const uint32_t maxBranches = (Storage::pageSize-sizeof(uint32_t)+sizeof(IndexKey)) / (sizeof(Address) + sizeof(IndexKey));
		static_assert(maxBranches >= 3, "Page size too small, at least 3 branches needed");
		static const uint32_t splitPoint32_t = (maxBranches + 1) / 2;
		uint32_t numBranches;

		IndexKey values[maxBranches - 1];
		Address children[maxBranches];

		inline void insert(uint32_t, IndexKey, Address, Address);
		inline void remove(uint32_t, Address);
		inline uint32_t length() const {return numBranches;}
	};

	struct LevelLocation {
		Address address;
		Address smallerSibling, greaterSibling;
		IndexKey smallerValue, greaterValue;
		uint8_t idx, max;

		inline bool hasMore() const{
			return idx < max;
		}
	};

	typedef mm::DynamicStack<LevelLocation, Allocator, BTREE_LOCATOR_LEVELS> Locator;

	class Iterator {
		friend BTree;
	protected:
		const IndexKey key;
		Locator locator;

		inline void updateSiblings(Node* index);
	public:
		Address currentAddress;
		Iterator(const IndexKey& k);

		inline bool hasNext() const;
	};

	template<class T>
	struct PlanOfAction {
		enum {
			mergeUp, mergeDown, redistGreater, redistSmaller
		}action;
		T* partner;
		uint32_t length;
	};

	//
	// Updaters
	//

	inline FailAddress createRootNode(RWSession& session, IndexKey hash, Address lower, Address higher, int32_t levels);
	inline FailAddress doUpdate(RWSession &session, Locator& locator, Address updatedAddress);
	inline FailAddress propagateUpdate(RWSession &session, Locator& locator, Address updatedAddress);
	inline FailAddress updateOne(RWSession& session, Locator& locator, Address updatedAddress);

	enum class UpdateDirection {Up, Down};
	inline FailAddress updateTwo(RWSession& session, Locator& locator, UpdateDirection dir, IndexKey newHash, Address updatedAddress, Address otherAddress);

	//
	// Iteration workers
	//

	template<class Comparator = FullComparator>
	inline ubiq::GenericError stepDown(ROSession &session, Iterator& iterator, Address address, uint32_t level);

	template<class Comparator = FullComparator>
	inline ubiq::GenericError step(ROSession &session, Iterator& iterator);

	template<class Comparator = FullComparator>
	inline ubiq::GenericError iterate(ROSession &session, Iterator& iterator);

	template <class IndexComparator, class KeyComparator, class MatchHandler>
	inline ubiq::GenericError checkTable(ROSession &session, const typename Storage::Address& address, Key &key, Value &value, MatchHandler &matchHandler);

	//
	// Mutation helpers
	//

	inline ubiq::FailPointer<Table> splitTable(RWSession& session, Table& table, uint32_t insIdx, const Key& key, Value value);
	inline FailAddress splitEntry(RWSession &session, Locator &pos, IndexKey hash, Address updatedAddress, Address newAddress);
	enum class MergeDirection {Up, Down};
	inline FailAddress mergeEntry(RWSession& session, Locator &pos, Address newAddress, MergeDirection direction, bool rootHasTwo);

	template<class T>
	inline ubiq::GenericError actionPlanner(ROSession &session, PlanOfAction<T> &plan, Locator&);

	template<bool updateAllowed, bool insertAllowed>
	inline ubiq::GenericError write(const Key &key, const Value &value);

	//
	// Internal data
	//

	uint32_t levels = 0;
	Address root = InvalidAddress;

	typedef mm::DynamicStack<Address, Allocator, BTREE_TRAVERSOR_LEVELS> Traversor;

	template<class ElementCallback>
	ubiq::GenericError traverse(RWSession &session, ElementCallback&& callback);

public:
	inline BTree() = default;
	inline BTree(Address root, int32_t levels): root(root), levels(levels) {}

	struct DefaultMatchHandler {
		inline bool onMatch(Element &e, Key& k, Value &v) {
			k = e.key;

			if(&v != 0)
				v = e.value;

			return false;
		}
	};

	template <class IndexComparator, class KeyComparator, class MatchHandler>
	ubiq::GenericError search(Key &key, Value &value, MatchHandler &matchHandler);

	template <class IndexComparator, class KeyComparator, class MatchHandler=DefaultMatchHandler>
	ubiq::GenericError search(Key &key, Value &value);

	inline ubiq::GenericError get(Key &key, Value &value = *(Value*)0);

	ubiq::GenericError put(const Key &key, const Value &value);
	ubiq::GenericError insert(const Key &key, const Value &value);
	ubiq::GenericError update(const Key &key, const Value &value);

	ubiq::GenericError remove(const Key &key, Value *value = 0);
	inline ubiq::GenericError purge();
	inline ubiq::GenericError relocate(Address&);
};

////////////////////////////////////////////////////////////////////////////////////////////

#include "Node.h"
#include "Locator.h"
#include "Table.h"
#include "Add.h"
#include "Remove.h"
#include "Search.h"
#include "Utility.h"

#endif /* BTREE_H_ */
