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

#include "1test/Test.h"
#include "1test/FailureInjector.h"

#include "BTreeParametrizedForTesting.h"
#include "BTreeTextValue.h"
#include "BTreeCommon.h"

#include "MockStorage.h"
#include "pet/test/MockAllocator.h"
#include "ubiquitous/Trace.h"

#include "DateKey.h"

#include <vector>
#include <iostream>
#include <algorithm>

#include <unistd.h>

using namespace std;

static constexpr const unsigned int elementSize = ((sizeof(DateKey) + 3) / 4 + (sizeof(TextValue) + 3) / 4) * 4;

typedef BTree<class Storage, DateKey, YearKey, TextValue, FailableAllocator> TestTree;
class Storage: public MockStorage<4 * elementSize, TestTree> {};

const DateKey birthDate			(1991, 10, 13, 3);
const DateKey progDate			(2003,  7, 12, 4);
const DateKey universityDate	(2010,  9, 1,  2);
const DateKey workDate			(2015,  2, 2,  1);
const DateKey thesisDate		(2015,  4, 28, 4);
const DateKey fsDate			(2015, 12, 14, 5);

const TextValue birthText		("A legend was born :)");
const TextValue progText      	("Discovered the art");
const TextValue universityText	("Worked hard for the title");
const TextValue workText      	("Enslaved :(");
const TextValue thesisText    	("Started work on thesis");
const TextValue fsText        	("Realized the need for a good fs");

/**
 * General insert/get sanity check
 */
static void insertOne(TestTree &tree) {

	static constexpr const char *str = "testwriting";

	TextValue value(str);
	DateKey today(2016, 8, 10);

	BTreeTestUtils::BTreeTestUtils::requireFailure(tree.get(today, value));			// should not contain
	CHECK(value == str);						// should not modify

	BTreeTestUtils::BTreeTestUtils::requireSucces(tree.insert(today, value));	// should be able to insert
	BTreeTestUtils::requireFailure(tree.insert(today, value));			// should not be able to insert again

	value = "";									// modify to be able to decide if the following works

	BTreeTestUtils::requireSucces(tree.get(today, value));				// should be able to get
	CHECK(value  == str);						// should be equal

	BTreeTestUtils::requireSucces(tree.remove(today));					// should be removable
}

/**
 * General update/get sanity check
 */
static void updateOne(TestTree &tree) {

	static constexpr const char *str = "testwriting";

	TextValue value("");
	DateKey today(2016, 8, 10);
	DateKey otherDay(2017, 1, 1);

	BTreeTestUtils::requireFailure(tree.update(today, value));			// should not be able to update before insert
	CHECK(value == "");						// should not change

	BTreeTestUtils::requireSucces(tree.insert(today, value));			// should be able to insert
	BTreeTestUtils::requireSucces(tree.get(today, value));				// should be able to get
	CHECK(value == "");						// should be equal

	BTreeTestUtils::requireFailure(tree.update(otherDay, value));		// should not be able to update if not present

	value = str;								// modify to be able to decide if the following works

	BTreeTestUtils::requireSucces(tree.update(today, value));			// should be able to update
	BTreeTestUtils::requireSucces(tree.get(today, value));				// should be able to get

	CHECK(value == str);						// should be equal

	BTreeTestUtils::requireSucces(tree.remove(today));					// should be removable
}

/**
 * General insert/remove sanity check
 */
static void insertRemove(TestTree &tree) {
	static constexpr const char *strFirstCheck = "testwriting";
	static constexpr const char *strDoubleCheck = "doublechecking";
	static constexpr const char *strThirdCheck = "verysafe";

	TextValue value(strFirstCheck);
	DateKey today(2016, 8, 10);

	BTreeTestUtils::requireFailure(tree.remove(today, &value));		// should not be able to remove when not contained
	CHECK(value == strFirstCheck);				// should not change if returns false

	BTreeTestUtils::requireSucces(tree.insert(today, value));			// should be able to put

	value = strDoubleCheck;						// modify to be able to decide if the following works

	DateKey otherDay(2017, 1, 1);
	BTreeTestUtils::requireFailure(tree.remove(otherDay));				// should not be able to remove if not added

	BTreeTestUtils::requireSucces(tree.remove(today, &value));			// should be able to remove
	CHECK(value == strFirstCheck);				// contents should be retrieved

	value = strThirdCheck;

	BTreeTestUtils::requireFailure(tree.remove(today, &value));		// should not be able to remove again
	CHECK(value == strThirdCheck);				// should not change
}

/**
 * Every get should fill the missing values of the key
 */
static void simpleSearch(TestTree &tree)
{
	// event rating not specified (default 0)
	DateKey key(workDate.year, workDate.month, workDate.day);
	TextValue value("");

	// should succeed, the entry was added by generator
	BTreeTestUtils::requireSucces(tree.get(key, value));

	// value should be OK
	CHECK(value == workText);

	// the key should be corrected
	CHECK(key.eventRating == workDate.eventRating);

	// null value reference should cause no problems
	BTreeTestUtils::requireSucces(tree.get(key, *(TextValue*)0));
}

typedef BTreeTestUtils::DummyComparator<TestTree::Element, DateKey> DummyElementComparator;
typedef BTreeTestUtils::DummyComparator<YearKey, YearKey> DummyIndexComparator;

struct EventRatingCheckMatchHandler {
	inline bool onMatch(TestTree::Element &e, DateKey& k, TextValue &v) {
		/**
		 * If keys do not match (according the default operator) then go on
		 */
		if(!(e.key.eventRating == k.eventRating))
			return true;

		/**
		 * Fill the possibly missing values
		 */
		k = e.key;

		if(&v != 0)
			v = e.value;

		return false;
	}
};

/**
 * Searching by non primary key
 *
 * Get an element for which the year and the rating is known,
 * the test element is chosen so that these are unique.
 */
static void nonPrimarySearch(TestTree &tree)
{
	// only year(indexed) and event rating specified
	DateKey key(workDate.year, 0, 0, workDate.eventRating);
	TextValue value("");

	BTreeTestUtils::requireSucces(tree.search<TestTree::FullComparator, DummyElementComparator, EventRatingCheckMatchHandler>(key, value));

	// should find by year and event rating
	CHECK(value == workText);

	// the key should be corrected
	CHECK(key.eventRating == workDate.eventRating);
}

/*
 *	Search by non primary key, without using the index.
 *
 *	In this case nothing but the event rating is known (the field that is not
 *	part of the primary key). Because of that we have to iterate through the
 *	whole tree (ie. not using the index) because we do not know the value by
 *	which we could find our way through the index.
 */
static void nonPrimaryNonIndexedSearch(TestTree &tree)
{
	// only event rating specified, indexed search can not be performed
	DateKey key(0, 0, 0, universityDate.eventRating);
	TextValue value("");

	BTreeTestUtils::requireSucces(tree.search<DummyIndexComparator, DummyElementComparator, EventRatingCheckMatchHandler>(key, value));

	// should find by event rating
	CHECK(value == universityText);

	// the key should be filled in with the missing values
	CHECK(key.eventRating == universityDate.eventRating);
}

/*
 *	Search by non primary key, without using the index, fetch all results.
 */
static void nonPrimaryNonIndexedMultipleResultSearch(TestTree &tree)
{
	// Match handler that removes the found item from the results vector
	struct MatchHandler {
		vector<TextValue> &results;
		MatchHandler(vector<TextValue> &results): results(results) {}

		inline bool onMatch(TestTree::Element &e, DateKey& k, TextValue &v)
		{
			/**
			 * If keys do not match (according the default operator) then go on
			 */
			if(!(e.key.eventRating > k.eventRating))
				return true;

			/**
			 * Otherwise
			 */
			auto it = std::find(results.begin(), results.end(), e.value);

			// The element should be present, as this is called only once for each item
			CHECK(it != results.end());
			results.erase(it);
			return true;
		}
	};

	// the results vector stores the expected results
	vector<TextValue> results{progText, thesisText, fsText};

	// the key only specifies a valid value for the eventRating
	// value (so again, indexed search is not possible)
	DateKey key(0, 0, 0, birthDate.eventRating);


	// The value argument is specified as null, as it is not needed for this case
	MatchHandler matchHandler(results);
	BTreeTestUtils::requireFailure(tree.search<DummyIndexComparator, DummyElementComparator>(key, *(TextValue*)0, matchHandler));

	// All the results should be consumed
	CHECK(results.size() == 0);
}

static void addNamed(TestTree &testTree)
{
	BTreeTestUtils::requireSucces(testTree.put(workDate, workText));
	BTreeTestUtils::requireSucces(testTree.put(fsDate, fsText));
	BTreeTestUtils::requireSucces(testTree.put(thesisDate, thesisText));
	BTreeTestUtils::requireSucces(testTree.put(birthDate, birthText));
	BTreeTestUtils::requireSucces(testTree.put(progDate, progText));
	BTreeTestUtils::requireSucces(testTree.put(universityDate, universityText));
}

static void removeNamed(TestTree &testTree)
{
	BTreeTestUtils::requireSucces(testTree.remove(thesisDate, NULL));
	BTreeTestUtils::requireSucces(testTree.remove(workDate, NULL));
	BTreeTestUtils::requireSucces(testTree.remove(fsDate, NULL));
	BTreeTestUtils::requireSucces(testTree.remove(universityDate, NULL));
	BTreeTestUtils::requireSucces(testTree.remove(birthDate, NULL));
	BTreeTestUtils::requireSucces(testTree.remove(progDate, NULL));
}

TEST_GROUP(BtreeDummyData) {
	TestTree tree;

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST_GROUP(BtreeMeaningfulData) {
	TestTree tree;

	TEST_SETUP() {
		pet::FailureInjector::disable();
		addNamed(tree);
		pet::FailureInjector::enable();
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST_GROUP(BtreeNoiseAddedData) {
	TestTree tree;
	static constexpr unsigned int primes[] = {2, 3, 5, 7};//{3, 5, 7, 11};

	TEST_SETUP() {
		pet::FailureInjector::disable();
		addNamed(tree);

		TextValue value("bullShit");

		/**
		 * Add sequential data in pseudo random order
		 */
		for(int year = primes[0]; year ; year = (year + primes[0]) % primes[3])
			for(int month = primes[1]; month ; month = (month + primes[1]) % primes[3])
				for(int day = primes[2]; day; day = (day + primes[2]) % primes[3])
					BTreeTestUtils::requireSucces(tree.insert(DateKey(1992 + year, 1 + month, 1 + day), value));
		pet::FailureInjector::enable();
	}

	TEST_TEARDOWN() {
		removeNamed(tree);
		/**
		 * Remove sequential data in pseudo random order (note the shuffled indices)
		 */

		pet::FailureInjector::disable();
		for(int year = primes[0]; year; year = (year + primes[0]) % primes[3])
			for(int month = primes[1]; month ; month = (month + primes[1]) % primes[3])
				for(int day = primes[2]; day; day = (day + primes[2]) % primes[3])
					BTreeTestUtils::requireSucces(tree.remove(DateKey(1992 + year, 1 + month, 1 + day), NULL));



		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(BtreeDummyData, InsertOne) {
	insertOne(tree);
}

TEST(BtreeDummyData, UpdateOne) {
	updateOne(tree);
}

TEST(BtreeDummyData, InsertRemove) {
	insertRemove(tree);
}

TEST(BtreeMeaningfulData, InsertOne) {
	insertOne(tree);
}

TEST(BtreeMeaningfulData, UpdateOne) {
	updateOne(tree);
}

TEST(BtreeMeaningfulData, InsertRemove) {
	insertRemove(tree);
}

TEST(BtreeMeaningfulData, SimpleSearch) {
	simpleSearch(tree);
}

TEST(BtreeMeaningfulData, NonPrimarySearch) {
	nonPrimarySearch(tree);
}

TEST(BtreeMeaningfulData, NonPrimaryNonIndexedSearch) {
	nonPrimaryNonIndexedSearch(tree);
}

TEST(BtreeMeaningfulData, NonPrimaryNonIndexedMultipleResultSearch) {
	nonPrimaryNonIndexedMultipleResultSearch(tree);
}

TEST(BtreeNoiseAddedData, InsertOneTest) {
	insertOne(tree);
}

TEST(BtreeNoiseAddedData, UpdateOneTest) {
	updateOne(tree);
}

TEST(BtreeNoiseAddedData, InsertRemove) {
	insertRemove(tree);
}

TEST(BtreeNoiseAddedData, SimpleSearch) {
	simpleSearch(tree);
}

TEST(BtreeNoiseAddedData, NonPrimarySearch) {
	nonPrimarySearch(tree);
}

TEST(BtreeNoiseAddedData, NonPrimaryNonIndexedSearch) {
	nonPrimaryNonIndexedSearch(tree);
}

TEST(BtreeNoiseAddedData, NonPrimaryNonIndexedMultipleResultSearch) {
	nonPrimaryNonIndexedMultipleResultSearch(tree);
}
