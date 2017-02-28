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

#include "CppUTest/TestHarness.h"

#include "BTreeParametrizedForTesting.h"
#include "KVTestKey.h"

#include "MockStorage.h"
#include "pet/test/MockAllocator.h"

#include "BTreeCommon.h"

#include <cstring>
#include <iostream>

constexpr const char* const keys[] = {
	"Cum",				"Nam",				"in",				"sapien",
	"amet,",			"interdum",			"vestibulum",		"consequat",
	"Curabitur",		"non.",				"eleifend",			"tellus",
	"quam",				"ultrices",			"lectus.",			"ullamcorper,",
	"felis",			"suscipit.",		"eu",				"bibendum",
	"finibus",			"In",				"magna",			"Quisque",
	"volutpat",			"feugiat.",			"penatibus",		"massa",
	"sociis",			"aliquam",			"lorem",			"echo",
	"ac",				"Lorem",			"nulla.",			"Maecenas",
	"sit",				"posuere",			"Suspendisse",		"amet",
	"nisl",				"aliquam,",			"ridiculus",		"dolor"
};

constexpr unsigned int nKeys = sizeof(keys)/sizeof(keys[0]);

const char* const values[] = {
	"natoque",			"vulputate,",		"parturient",		"lacus,",
	"quam,",			"rhoncus.",			"nibh",				"ligula",
	"congue",			"magnis",			"aliquet",			"velit,",
	"Sed",				"lacus",			"nascetur",			"enim",
	"ipsum,",			"ut,",				"consectetur",		"ipsum",
	"in,",				"nunc",				"dictum",			"tempus.",
	"ultricies.",		"montes,",			"mus",				"sed",
	"pulvinar",			"non",				"elit.",			"sem",
	"et",				"imperdiet",		"laoreet",			"commodo",
	"adipiscing",		"suscipit",			"sodales",			"dis",
	"blandit",			"Donec",			"sagittis",			"turpis"
};

constexpr unsigned int nValues = sizeof(values)/sizeof(values[0]);

namespace KeyLengthInfo {
	struct countLonger {
		static constexpr unsigned int f(unsigned int than, unsigned int idx = nKeys-1) {
			return (idx ? f(than, idx - 1) : 0) + (strlen(keys[idx]) > than);
		}
	};

	struct countShorterOrEqual {
		static constexpr unsigned int f(unsigned int than, unsigned int idx = nKeys-1) {
			return (idx ? f(than, idx - 1) : 0) + (strlen(keys[idx]) <= than);
		}
	};

	constexpr unsigned int max(unsigned int a, unsigned int b) {
		return (a > b) ? a : b;
	}

	constexpr unsigned int findMax(unsigned int idx = nKeys-1) {
		return (idx == 0) ? strlen(keys[0]) : max(findMax(idx - 1), strlen(keys[idx]));
	}

	template<class f>
	struct sequence {
		template <unsigned int N, unsigned int... rest>
		struct iterate {
			constexpr static auto &array = iterate<N-1, N, rest...>::array;
		};

		template <unsigned int... rest>
		struct iterate<-1u, rest...> {
			constexpr static unsigned int array[] = {f::f(rest)...};
		};
	};

	template<class f>
	template <unsigned int... rest>
	constexpr unsigned int sequence<f>::iterate<-1u, rest...>::array[];

	constexpr auto &nLonger = sequence<countLonger>::iterate<findMax()>::array;
	constexpr auto &nShorterOrEqual = sequence<countShorterOrEqual>::iterate<findMax()>::array;
};

typedef BTree<class Storage, KVTestKey, KVTestIndexKey, const char*, FailableAllocator> TestTree;
class Storage: public MockStorage<8*sizeof(void*), TestTree> {};

static void simpleSearch(TestTree &tree)
{
	for(unsigned int i=0; i<nKeys; i++) {
		KVTestKey key(keys[i]);
		const char* value = NULL;

		pet::GenericError ret = tree.get(key, value);
		CHECK(!ret.failed());
		CHECK(ret);

		CHECK(strcmp(key.name, keys[i]) == 0);

		CHECK(value == values[i]);
	}
}

struct CountingMatchHandler {
	unsigned int &count;

	inline CountingMatchHandler(unsigned int &count): count(count) {
		count = 0;
	}

	inline bool onMatch(TestTree::Element &e, KVTestKey& k, const char* &v) {
		count++;
		return true;
	}
};

static void countLonger(TestTree &tree)
{
	struct LongerIndexComparator {
		const KVTestIndexKey &key;
		inline LongerIndexComparator(const KVTestIndexKey &key): key(key){}

		inline bool greater(const KVTestIndexKey& than) const {
			return false;
		}

		inline bool matches(const KVTestIndexKey& to) const {
			return to.length > key.length;
		}
	};

	struct LongerElementComparator {
		const KVTestKey &key;
		inline LongerElementComparator(const KVTestKey &key): key(key){}

		inline bool greater(const TestTree::Element& than) const {
			return false;
		}

		inline bool matches(const TestTree::Element& to) const {
			return strlen(to.key.name) > strlen(key.name);
		}
	};

	static char temp[KeyLengthInfo::findMax()+1];

	for(unsigned int length = 0; length < KeyLengthInfo::findMax(); length++) {
		for(unsigned int i=0; i<length; i++)
			temp[i] = '?';

		temp[length] = '\0';

		unsigned int count;
		KVTestKey key(temp);

		CountingMatchHandler countingMatchHandler(count);
		pet::GenericError ret = tree.search<LongerIndexComparator, LongerElementComparator>(key, *(const char**)0, countingMatchHandler);
		CHECK(!ret.failed());
		CHECK(!ret);

		CHECK(count == KeyLengthInfo::nLonger[length]);
	}
}

static void countShorterOrEqual(TestTree &tree)
{
	struct ShorterOrEqualIndexComparator {
		const KVTestIndexKey &key;
		inline ShorterOrEqualIndexComparator(const KVTestIndexKey &key): key(key){}

		inline bool greater(const KVTestIndexKey& than) const {
			return than.length > key.length;
		}

		inline bool matches(const KVTestIndexKey& to) const {
			return to.length <= key.length;
		}
	};

	struct ShorterOrEqualElementComparator {
		const KVTestKey &key;
		inline ShorterOrEqualElementComparator(const KVTestKey &key): key(key){}

		inline bool greater(const TestTree::Element& than) const {
			return strlen(than.key.name) > strlen(key.name);
		}

		inline bool matches(const TestTree::Element& to) const {
			return strlen(to.key.name) <= strlen(key.name);
		}
	};

	static char temp[KeyLengthInfo::findMax()+1];

	for(unsigned int length = 0; length < KeyLengthInfo::findMax(); length++) {
		for(unsigned int i=0; i<length; i++)
			temp[i] = '?';

		temp[length] = '\0';

		unsigned int count;
		KVTestKey key(temp);

		CountingMatchHandler countingMatchHandler(count);
		pet::GenericError ret = tree.search<ShorterOrEqualIndexComparator, ShorterOrEqualElementComparator>(key, *(const char**)0, countingMatchHandler);
		CHECK(!ret.failed());
		CHECK(!ret);

		CHECK(count == KeyLengthInfo::nShorterOrEqual[length]);
	}
}

TEST_GROUP(BTreeKV) {
	TestTree tree;
	static constexpr unsigned int coPrimeToNKeys1 = 23;
	static constexpr unsigned int coPrimeToNKeys2 = 19;

	TEST_SETUP() {
		INHIBIT_FAILURE_INJECTION_FOR_STATIC_SOURCE(FailableAllocator);
		DISABLE_FAILURE_INJECTION_TEMPORARILY();

		int i = 0;
		do {
			i = (i + coPrimeToNKeys1) % nKeys;
			pet::GenericError ret = tree.put(keys[i], values[i]);
			CHECK(!ret.failed());
			CHECK(ret);
		}while(i);

		ENABLE_FAILURE_INJECTION_TEMPORARILY();
	}

	TEST_TEARDOWN() {
		DISABLE_FAILURE_INJECTION_TEMPORARILY();

		int i = 0;
		do {
			i = (i + coPrimeToNKeys2) % nKeys;
			pet::GenericError ret = tree.remove(keys[i]);
			CHECK(!ret.failed());
			CHECK(ret);
		}while(i);
		ENABLE_FAILURE_INJECTION_TEMPORARILY();

		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(BTreeKV, SimpleSearch) {
	simpleSearch(tree);
}

TEST(BTreeKV, CountLonger) {
	countLonger(tree);
}

TEST(BTreeKV, CountShorterOrEqual) {
	countShorterOrEqual(tree);
}
