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

#include "BTreeParametrizedForTesting.h"

#include "MockStorage.h"
#include "pet/test/MockAllocator.h"

#include "BTreeCommon.h"

#include "SimpleKey.h"

#include <algorithm>

typedef BTree<class Storage, Key, uintptr_t, uintptr_t, FailableAllocator> TestTree;
constexpr auto elemSize = 3 * (sizeof(uintptr_t) + sizeof(Key));
constexpr auto indexSize = 3 * sizeof(void*) + 2 * sizeof(uintptr_t) + sizeof(uint32_t);
class Storage: public MockStorage<(indexSize > elemSize) ? indexSize : elemSize, TestTree> {};

TEST_GROUP(SimpleEmptyTree) {
	TestTree tree;

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleEmptyTree, AddOne) {
	BTreeTestUtils::requireSucces(tree.insert(1, 1));
	BTreeTestUtils::requireKeys(tree, {Key(1)});
}

TEST(SimpleEmptyTree, DontAddOne) {
	BTreeTestUtils::requireSucces(tree.insert(1, 1));
	BTreeTestUtils::requireFailure(tree.insert(1, 2));
}

TEST(SimpleEmptyTree, UpdateOne) {
	BTreeTestUtils::requireSucces(tree.insert(1, 1));
	BTreeTestUtils::requireSucces(tree.update(1, 2));
}

TEST(SimpleEmptyTree, DontUpdateOne) {
	BTreeTestUtils::requireFailure(tree.update(1, 2));
	BTreeTestUtils::requireKeysAlways(tree, {});
}

TEST(SimpleEmptyTree, Search) {
	uintptr_t value;
	Key key(2);
	BTreeTestUtils::requireFailure(tree.get(key, value));
	BTreeTestUtils::requireKeysAlways(tree, {});
}

TEST(SimpleEmptyTree, Traverse) {
	BTreeTestUtils::TraverseCounter<Storage, TestTree, 1> cb;

	BTreeTestUtils::requireFailure(BTreeTestUtils::traverse(tree, cb));
	CHECK(cb.counts[0] == 0);
}

TEST_GROUP(SimpleFirstLayerTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));
		BTreeTestUtils::requireSucces(tree.insert(8, 8));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleFirstLayerTree, AddFourth) {
	BTreeTestUtils::requireSucces(tree.insert(9, 9));
	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(8), Key(9)});
}

TEST(SimpleFirstLayerTree, AddThird) {
	BTreeTestUtils::requireSucces(tree.insert(6, 6));
	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(6), Key(8)});
}

TEST(SimpleFirstLayerTree, AddSecond) {
	BTreeTestUtils::requireSucces(tree.insert(3, 3));
	BTreeTestUtils::requireKeys(tree, {Key(2), Key(3), Key(5), Key(8)});
}

TEST(SimpleFirstLayerTree, AddFirst) {
	BTreeTestUtils::requireSucces(tree.insert(0u, 0));
	BTreeTestUtils::requireKeys(tree, {Key(0), Key(2), Key(5), Key(8)});
}

TEST(SimpleFirstLayerTree, DontRemove) {
	BTreeTestUtils::requireFailure(tree.remove(0u));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8)});
}

TEST(SimpleFirstLayerTree, Search) {
	uintptr_t value;
	Key key(2);
	BTreeTestUtils::requireSucces(tree.get(key, value));
	CHECK(value==2);
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8)});
}

TEST(SimpleFirstLayerTree, SearchDontFind) {
	uintptr_t value;
	Key key(13);
	BTreeTestUtils::requireFailure(tree.get(key, value));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8)});
}

TEST(SimpleFirstLayerTree, Traverse) {
	BTreeTestUtils::TraverseCounter<Storage, TestTree ,1> cb;

	BTreeTestUtils::requireFailure(BTreeTestUtils::traverse(tree, cb));
	CHECK(cb.counts[0] == 1);
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8)});
}

TEST_GROUP(SimpleSplitTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));
		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleSplitTree, RemoveFourth) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(11, &value));
	CHECK(value == 11);
	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(8)});
}

TEST(SimpleSplitTree, RemoveThird) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(8, &value));
	CHECK(value == 8);
	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(11)});
}

TEST(SimpleSplitTree, RemoveSecond) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(5, &value));
	CHECK(value == 5);
	BTreeTestUtils::requireKeys(tree, {Key(2), Key(8), Key(11)});
}

TEST(SimpleSplitTree, RemoveFirst) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(2, &value));
	CHECK(value == 2);
	BTreeTestUtils::requireKeys(tree, {Key(5), Key(8), Key(11)});
}

TEST(SimpleSplitTree, DontRemove) {
	BTreeTestUtils::requireFailure(tree.remove(0u));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11)});
}

TEST(SimpleSplitTree, RemoveAll) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(11, &value));
	CHECK(value == 11);
	BTreeTestUtils::requireSucces(tree.remove(8, &value));
	CHECK(value == 8);
	BTreeTestUtils::requireSucces(tree.remove(5, &value));
	CHECK(value == 5);
	BTreeTestUtils::requireSucces(tree.remove(2, &value));
	CHECK(value == 2);
}

TEST(SimpleSplitTree, Grow0) {
	BTreeTestUtils::requireSucces(tree.insert(0u, 0));
	BTreeTestUtils::requireSucces(tree.insert(1, 1));
}

TEST(SimpleSplitTree, Grow1) {
	BTreeTestUtils::requireSucces(tree.insert(3, 3));
	BTreeTestUtils::requireSucces(tree.insert(4, 4));
}


TEST(SimpleSplitTree, Grow2) {
	BTreeTestUtils::requireSucces(tree.insert(6, 6));
	BTreeTestUtils::requireSucces(tree.insert(7, 7));
}

TEST(SimpleSplitTree, Grow4) {
	BTreeTestUtils::requireSucces(tree.insert(9, 9));
	BTreeTestUtils::requireSucces(tree.insert(10, 10));
}

TEST(SimpleSplitTree, Grow5) {
	BTreeTestUtils::requireSucces(tree.insert(12, 12));
	BTreeTestUtils::requireSucces(tree.insert(13, 13));
}

TEST(SimpleSplitTree, Update0) {
	BTreeTestUtils::requireSucces(tree.update(2, 0));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11)});
}

TEST(SimpleSplitTree, Update1) {
	BTreeTestUtils::requireSucces(tree.update(5, 0));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11)});
}

TEST(SimpleSplitTree, Update2) {
	BTreeTestUtils::requireSucces(tree.update(8, 0));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11)});
}

TEST(SimpleSplitTree, Update3) {
	BTreeTestUtils::requireSucces(tree.update(11, 0));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11)});
}

TEST(SimpleSplitTree, DontGrowMore) {
	BTreeTestUtils::requireSucces(tree.insert(9, 9));
	BTreeTestUtils::requireSucces(tree.remove(8));
	BTreeTestUtils::requireSucces(tree.insert(8, 8));
}

TEST(SimpleSplitTree, Traverse) {
	BTreeTestUtils::TraverseCounter<Storage, TestTree ,2> cb;

	BTreeTestUtils::requireFailure(BTreeTestUtils::traverse(tree, cb));
	CHECK(cb.counts[1] == 1);
	CHECK(cb.counts[0] == 2);
}

TEST_GROUP(SimpleFrontHeavySplitTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));
		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));
		BTreeTestUtils::requireSucces(tree.insert(1, 1));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleFrontHeavySplitTree, RedistSmaller1) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(8, &value));
	CHECK(value == 8);

	BTreeTestUtils::requireKeys(tree, {Key(1), Key(2), Key(5), Key(11)});
}

TEST(SimpleFrontHeavySplitTree, RedistSmaller2) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(11, &value));
	CHECK(value == 11);

	BTreeTestUtils::requireKeys(tree, {Key(1), Key(2), Key(5), Key(8)});
}


TEST_GROUP(SimpleBackHeavySplitTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));
		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));
		BTreeTestUtils::requireSucces(tree.insert(12, 12));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleBackHeavySplitTree, RedistGreater1) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(2, &value));
	CHECK(value == 2);

	BTreeTestUtils::requireKeys(tree, {Key(5), Key(8), Key(11), Key(12)});
}

TEST(SimpleBackHeavySplitTree, RedistGreater2) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(5, &value));
	CHECK(value == 5);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(8), Key(11), Key(12)});
}

TEST(SimpleBackHeavySplitTree, SimpleRemove) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(8, &value));
	CHECK(value == 8);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(11), Key(12)});
}

TEST_GROUP(SimpleFullTwoLayerTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));

		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));

		BTreeTestUtils::requireSucces(tree.insert(14, 14));
		BTreeTestUtils::requireSucces(tree.insert(17, 17));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleFullTwoLayerTree, RedistGreater1) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(2, &value));
	CHECK(value == 2);
}

TEST(SimpleFullTwoLayerTree, Grow0) {
	BTreeTestUtils::requireSucces(tree.insert(0u, 0));
	BTreeTestUtils::requireSucces(tree.insert(1, 1));
}

TEST(SimpleFullTwoLayerTree, Grow1) {
	BTreeTestUtils::requireSucces(tree.insert(3, 3));
	BTreeTestUtils::requireSucces(tree.insert(4, 4));
}

TEST(SimpleFullTwoLayerTree, Grow2) {
	BTreeTestUtils::requireSucces(tree.insert(6, 6));
	BTreeTestUtils::requireSucces(tree.insert(7, 7));
}

TEST(SimpleFullTwoLayerTree, Grow4) {
	BTreeTestUtils::requireSucces(tree.insert(9, 9));
	BTreeTestUtils::requireSucces(tree.insert(10, 10));
}

TEST(SimpleFullTwoLayerTree, Grow5) {
	BTreeTestUtils::requireSucces(tree.insert(12, 12));
	BTreeTestUtils::requireSucces(tree.insert(13, 13));
}

TEST(SimpleFullTwoLayerTree, Grow6) {
	BTreeTestUtils::requireSucces(tree.insert(15, 15));
	BTreeTestUtils::requireSucces(tree.insert(16, 16));
}

TEST(SimpleFullTwoLayerTree, Grow7) {
	BTreeTestUtils::requireSucces(tree.insert(18, 18));
	BTreeTestUtils::requireSucces(tree.insert(19, 19));
}

TEST(SimpleFullTwoLayerTree, RedistSmaller1) {
	BTreeTestUtils::requireSucces(tree.insert(7, 7));
	BTreeTestUtils::requireSucces(tree.remove(11));
}

TEST(SimpleFullTwoLayerTree, RedistGreater2) {
	BTreeTestUtils::requireSucces(tree.insert(18, 18));
	BTreeTestUtils::requireSucces(tree.insert(7, 7));
	BTreeTestUtils::requireSucces(tree.remove(11));
}

TEST(SimpleFullTwoLayerTree, DontInsert) {
	BTreeTestUtils::requireFailure(tree.insert(17, 0));

	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17)});
}

TEST(SimpleFullTwoLayerTree, Traverse) {
	BTreeTestUtils::TraverseCounter<Storage, TestTree ,2> cb;

	BTreeTestUtils::requireFailure(BTreeTestUtils::traverse(tree, cb));
	CHECK(cb.counts[1] == 1);
	CHECK(cb.counts[0] == 3);

	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17)});
}


TEST_GROUP(SimpleBarelyThreeLayerTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));

		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));

		BTreeTestUtils::requireSucces(tree.insert(14, 14));
		BTreeTestUtils::requireSucces(tree.insert(17, 17));

		BTreeTestUtils::requireSucces(tree.insert(20, 20));
		BTreeTestUtils::requireSucces(tree.insert(23, 23));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleBarelyThreeLayerTree, CollapseSmallSmall) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(2, &value));
	CHECK(value == 2);

	BTreeTestUtils::requireKeys(tree, {Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23)});
}

TEST(SimpleBarelyThreeLayerTree, CollapseBigSmall) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(8, &value));
	CHECK(value == 8);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(11), Key(14), Key(17), Key(20), Key(23)});
}

TEST(SimpleBarelyThreeLayerTree, CollapseSmallBig) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(14, &value));
	CHECK(value == 14);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(8), Key(11), Key(17), Key(20), Key(23)});
}

TEST(SimpleBarelyThreeLayerTree, CollapseBigBig) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(20, &value));
	CHECK(value == 20);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17), Key(23)});
}

TEST(SimpleBarelyThreeLayerTree, DontGrowMore) {
	BTreeTestUtils::requireSucces(tree.insert(15, 15));
	BTreeTestUtils::requireSucces(tree.remove(14));
	BTreeTestUtils::requireSucces(tree.insert(14,14));
}

TEST(SimpleBarelyThreeLayerTree, Traverse) {
	BTreeTestUtils::TraverseCounter<Storage, TestTree ,3> cb;

	BTreeTestUtils::requireFailure(BTreeTestUtils::traverse(tree, cb));

	CHECK(cb.counts[2] == 1);
	CHECK(cb.counts[1] == 2);
	CHECK(cb.counts[0] == 4);

	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23)});
}

TEST(SimpleBarelyThreeLayerTree, RelocateFirst) {
	Storage::Address page;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 1, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}

TEST(SimpleBarelyThreeLayerTree, RelocateSecond) {
	Storage::Address page;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 2, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}

TEST(SimpleBarelyThreeLayerTree, RelocateThird) {
	Storage::Address page;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 3, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}

TEST(SimpleBarelyThreeLayerTree, RelocateFourth) {
	Storage::Address page;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 4, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}

TEST(SimpleBarelyThreeLayerTree, RelocateFifth) {
	Storage::Address page;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 5, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}


TEST(SimpleBarelyThreeLayerTree, RelocateSixth) {
	Storage::Address page = Storage::InvalidAddress;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 6, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}

TEST(SimpleBarelyThreeLayerTree, RelocateSeventh) {
	Storage::Address page = Storage::InvalidAddress;
	BTreeTestUtils::requireFailure(BTreeTestUtils::nthAddress(tree, 7, page));
	CHECK(page != Storage::InvalidAddress);
	tree.allowUnnecessary = true;
	BTreeTestUtils::requireSucces(tree.relocate(page));
}

TEST_GROUP(SimpleFrontHeavyThreeLayerTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));

		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));

		BTreeTestUtils::requireSucces(tree.insert(20, 20));
		BTreeTestUtils::requireSucces(tree.insert(23, 23));

		BTreeTestUtils::requireSucces(tree.insert(26, 26));
		BTreeTestUtils::requireSucces(tree.insert(29, 29));

		BTreeTestUtils::requireSucces(tree.insert(14, 14));
		BTreeTestUtils::requireSucces(tree.insert(17, 17));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};


TEST(SimpleFrontHeavyThreeLayerTree, RedistSmaller) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(29, &value));
	CHECK(value == 29);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23), Key(26)});
}

TEST(SimpleFrontHeavyThreeLayerTree, Traverse) {
	BTreeTestUtils::TraverseCounter<Storage, TestTree ,3> cb;

	BTreeTestUtils::requireFailure(BTreeTestUtils::traverse(tree, cb));

	CHECK(cb.counts[2] == 1);
	CHECK(cb.counts[1] == 2);
	CHECK(cb.counts[0] == 5);

	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23), Key(26), Key(29)});
}

TEST_GROUP(SimpleBackHeavyThreeLayerTree) {
	TestTree tree;

	TEST_SETUP() {
		BTreeTestUtils::requireSucces(tree.insert(2, 2));
		BTreeTestUtils::requireSucces(tree.insert(5, 5));

		BTreeTestUtils::requireSucces(tree.insert(8, 8));
		BTreeTestUtils::requireSucces(tree.insert(11, 11));

		BTreeTestUtils::requireSucces(tree.insert(14, 14));
		BTreeTestUtils::requireSucces(tree.insert(17, 17));

		BTreeTestUtils::requireSucces(tree.insert(20, 20));
		BTreeTestUtils::requireSucces(tree.insert(23, 23));

		BTreeTestUtils::requireSucces(tree.insert(26, 26));
		BTreeTestUtils::requireSucces(tree.insert(29, 29));
	}

	TEST_TEARDOWN() {
		BTreeTestUtils::finalCheckAndCleanup(tree);
	}
};

TEST(SimpleBackHeavyThreeLayerTree, RedistGreater1) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(2, &value));
	CHECK(value == 2);

	BTreeTestUtils::requireKeys(tree, {Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23), Key(26), Key(29)});
}

TEST(SimpleBackHeavyThreeLayerTree, RedistGreater2) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(8, &value));
	CHECK(value == 8);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(11), Key(14), Key(17), Key(20), Key(23), Key(26), Key(29)});
}

TEST(SimpleBackHeavyThreeLayerTree, RedistGreater3) {
	uintptr_t value;
	BTreeTestUtils::requireSucces(tree.remove(14, &value));
	CHECK(value == 14);

	BTreeTestUtils::requireKeys(tree, {Key(2), Key(5), Key(8), Key(11), Key(17), Key(20), Key(23), Key(26), Key(29)});
}

TEST(SimpleBackHeavyThreeLayerTree, Search) {
	uintptr_t value;
	Key key(14);
	BTreeTestUtils::requireSucces(tree.get(key, value));
	CHECK(value == 14);

	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23), Key(26), Key(29)});
}

TEST(SimpleBackHeavyThreeLayerTree, SearchDontFind) {
	uintptr_t value;
	Key key(13);
	BTreeTestUtils::requireFailure(tree.get(key, value));
	BTreeTestUtils::requireKeysAlways(tree, {Key(2), Key(5), Key(8), Key(11), Key(14), Key(17), Key(20), Key(23), Key(26), Key(29)});
}

