/*******************************************************************************
 *
 * Copyright (c) 2016 Seller Tamás. All rights reserved.
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

#ifndef COMMON_H_
#define COMMON_H_

#include <set>

#include "CppUTest/TestHarness.h"
#include "FailureInjectorPlugin.h"

#include "BTreeParametrizedForTesting.h"

#include "MockStorage.h"
#include "pet/test/MockAllocator.h"

struct BTreeTestUtils {
	template <class TestTree>
	using Table = typename TestTree::Table;

	template <class TestTree>
	using Element = typename TestTree::Element;

	template<class Storage, class TestTree, unsigned int N>
	struct TraverseCounter {
		unsigned int counts[N] = {0,};
		typename Storage::Address operator()(typename Storage::Address addr, unsigned int level, const typename TestTree::Traversor & parents) {
			CHECK(level < N);
			counts[level]++;
			return addr;
		}
	};

	template <class Storage, class Key, class IndexKey, class Value, class Allocator, class Callback>
	static ubiq::GenericError traverse(BTree<Storage, Key, IndexKey, Value, Allocator> &tree, Callback&& callback) {
		typename Storage::ReadWriteSession session(&tree);
		auto ret = tree.traverse<Callback>(session, callback);
		tree.Storage::closeReadWriteSession(session);
		return ret;
	}

	template <class Storage, class Key, class IndexKey, class Value, class Allocator>
	static ubiq::GenericError nthAddress(BTree<Storage, Key, IndexKey, Value, Allocator> &tree, unsigned int n, typename Storage::Address &retAddr) {
		typename Storage::ReadWriteSession session(&tree);
		auto ret = tree.traverse(session, [&](typename Storage::Address addr, unsigned int level, const typename BTree<Storage, Key, IndexKey, Value, Allocator>::Traversor & parents) {
			if(!--n) {
				retAddr = addr;
				return Storage::InvalidAddress;
			}
			return addr;
		});

		tree.Storage::closeReadWriteSession(session);
		return ret;
	}

	template <class Storage, class Key, class IndexKey, class Value, class Allocator>
	static inline void finalCheckAndCleanup(BTree<Storage, Key, IndexKey, Value, Allocator> &tree)
	{
		using Traversor = typename BTree<Storage, Key, IndexKey, Value, Allocator>::Traversor;

		DISABLE_FAILURE_INJECTION_TEMPORARILY();
		typename Storage::ReadWriteSession session(&tree);

		CHECK(!tree.traverse(session, [&](typename Storage::Address addr, int level,
				const Traversor &parents) {
			CHECK(((typename Storage::PageBuffer*)addr)->level == level);
			return addr;
		}).failed());

		tree.Storage::closeReadWriteSession(session);
		ENABLE_FAILURE_INJECTION_TEMPORARILY();

		bool canBeMessy = tree.purge().failed();
		bool ok = Storage::isClean();
		if(canBeMessy)
			ok = true;

		CHECK_ALWAYS(ok);

		CHECK_ALWAYS(FailableAllocator::allFreed());
	}

	template<class Result>
	static void requireSucces(Result status) {
		CHECK(!status.failed());
		CHECK(status);
	}

	template<class Result>
	static void requireFailure(Result status) {
		CHECK(!status.failed());
		CHECK(!status);
	}

	/**
	 * Match everything comparator
	 */
	template <class T, class U>
	class DummyComparator {
		public:
			inline DummyComparator(const U&){}

			inline bool greater(const T& subject) {
				return false;
			}

			inline bool matches(const T& subject) {
				return true;
			}
	};

	template <class Storage, class Key, class IndexKey, class Value, class Allocator>
	static inline bool matchKeys(BTree<Storage, Key, IndexKey, Value, Allocator> &tree,
			std::initializer_list<Key> reqContents)
	{
		typedef typename BTree<Storage, Key, IndexKey, Value, Allocator>::Element Element;
		typedef ubiq::GenericError Result;
		typedef DummyComparator<Element, Key> KeyComparator;
		typedef DummyComparator<IndexKey, IndexKey> IndexComparator;

		struct MatchHandler {
			std::set<Key> contents;

			inline bool onMatch(Element &e, Key& k, Value &v) {
				contents.insert(e.key);
				return true;
			}
		};

		MatchHandler enumerator;
		Key key(Key::InvalidKey);

		Result res = tree.template search<IndexComparator, KeyComparator, MatchHandler>(key, *((Value*)0), enumerator);
		CHECK(!res.failed());
		// WTF syntax? XD "template disambiguator", Paragraph 14.2/4 of the C++11 Standard.

		auto it = reqContents.begin();

		for(Key x: enumerator.contents){
			if(it == reqContents.end() || *it != x)
				return false;

			++it;
		}

		return it == reqContents.end();
	}

	template <class Storage, class Key, class IndexKey, class Value, class Allocator>
	static inline void requireKeys(BTree<Storage, Key, IndexKey, Value, Allocator> &tree,
			std::initializer_list<Key> reqContents)
	{
		DISABLE_FAILURE_INJECTION_TEMPORARILY();
		CHECK(matchKeys(tree, reqContents));
		ENABLE_FAILURE_INJECTION_TEMPORARILY();
	}

	template <class Storage, class Key, class IndexKey, class Value, class Allocator>
	static inline void requireKeysOnFailure(BTree<Storage, Key, IndexKey, Value, Allocator> &tree,
			std::initializer_list<Key> reqContents)
	{
		DISABLE_FAILURE_INJECTION_TEMPORARILY();
		CHECK_ON_FAILURE(matchKeys(tree, reqContents));
		ENABLE_FAILURE_INJECTION_TEMPORARILY();
	}

	template <class Storage, class Key, class IndexKey, class Value, class Allocator>
	static inline void requireKeysAlways(BTree<Storage, Key, IndexKey, Value, Allocator> &tree,
			std::initializer_list<Key> reqContents)
	{
		DISABLE_FAILURE_INJECTION_TEMPORARILY();
		CHECK_ALWAYS(matchKeys(tree, reqContents));
		ENABLE_FAILURE_INJECTION_TEMPORARILY();
	}
};

#endif /* COMMON_H_ */
