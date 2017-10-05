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
#include "1test/Mock.h"

#include "1test/FailureInjector.h"

#include "pet/test/MockAllocator.h"
#include "MockFs.h"

#include "util/Path.h"

TEST_GROUP(FooBarStraight) {
	struct TestData{
		MockFs fs;
		const char *foo, *bar;

		TestData(): foo("foo"), bar("bar") {
			MockFs::Node node;
			CHECK(!fs.fetchRoot(node).failed());
			CHECK(!fs.newDirectory(node, foo, foo + strlen(foo)).failed());
			CHECK(!fs.newFile(node, bar, bar + strlen(bar)).failed());
		}
	};

	TestData *test;

	TEST_SETUP() {
		pet::FailureInjector::disable();
		test = new TestData;
		pet::FailureInjector::enable();
	}

	TEST_TEARDOWN() {
		pet::FailureInjector::disable();
		delete test;
		pet::FailureInjector::enable();
	}
};

TEST(FooBarStraight, findFoo) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;

	pet::GenericError res = path.append(test->fs, node, "/foo");
	CHECK(!res.failed());
	if(!res.failed()) {
		const char *start, *end;
		node.getName(start, end);
		CHECK(memcmp(start, test->foo, strlen(test->foo)) == 0);
	}
}

TEST(FooBarStraight, findBar) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;

	pet::GenericError res = path.append(test->fs, node, "/foo/bar");
	CHECK(!res.failed());
	if(!res.failed()) {
		const char *start, *end;
		node.getName(start, end);
		CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
	}
}

TEST(FooBarStraight, findBarMultipart) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;

	const char* pathName = "/foo/bar";
	pet::GenericError res = path.append(test->fs, node, pathName, pathName+4);
	CHECK(!res.failed());
	if(!res.failed()) {
		pet::GenericError res = path.append(test->fs, node, pathName+4, pathName+8);
		CHECK(!res.failed());
		if(!res.failed()) {
			const char *start, *end;
			node.getName(start, end);
			CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
		}
	}
}

TEST(FooBarStraight, findFooStepBack) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;

	pet::GenericError res = path.append(test->fs, node, "/foo/../foo");
	CHECK(!res.failed());
	if(!res.failed()) {
		const char *start, *end;
		node.getName(start, end);
		CHECK(memcmp(start, test->foo, strlen(test->foo)) == 0);
	}
}

TEST(FooBarStraight, failSillyStepBack) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(path.append(test->fs, node, "/foo/bar/..").failed());
}

TEST(FooBarStraight, failOneChar) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(path.append(test->fs, node, "/f").failed());
}

TEST(FooBarStraight, failDotOneChar) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(path.append(test->fs, node, "/.p").failed());
}


TEST(FooBarStraight, failTwoChar) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(path.append(test->fs, node, "/fo").failed());
}

TEST(FooBarStraight, failSillyDot) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(path.append(test->fs, node, "/foo/bar/.").failed());
}

TEST(FooBarStraight, funnyPath) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	pet::GenericError res = path.append(test->fs, node, "//../..///foo/../../foo/.//bar/");
	CHECK(!res.failed());
	if(!res.failed()) {
		const char *start, *end;
		node.getName(start, end);
		CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
	}
}

TEST(FooBarStraight, twoStep) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(!path.append(test->fs, node, "/foo").failed());
	CHECK(!path.append(test->fs, node, "/bar").failed());
	const char *start, *end;
	node.getName(start, end);
	CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
}

TEST(FooBarStraight, twoStepNoInitialSlash) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(!path.append(test->fs, node, "foo").failed());
	CHECK(!path.append(test->fs, node, "bar").failed());
	const char *start, *end;
	node.getName(start, end);
	CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
}

TEST(FooBarStraight, multipleDummySteps) {
	Path<MockFs, FailableAllocator> path;
	MockFs::Node node;
	CHECK(!path.append(test->fs, node, "/").failed());
	CHECK(!path.append(test->fs, node, "foo").failed());
	CHECK(!path.append(test->fs, node, "../").failed());
	CHECK(!path.append(test->fs, node, "foo").failed());
	CHECK(!path.append(test->fs, node, "/..").failed());
	CHECK(!path.append(test->fs, node, "foo").failed());
	CHECK(!path.append(test->fs, node, "bar").failed());
	const char *start, *end;
	node.getName(start, end);
	CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
}

TEST(FooBarStraight, copiesIndependent) {
	Path<MockFs, FailableAllocator> onePath;
	Path<MockFs, FailableAllocator> otherPath;
	MockFs::Node node;
	CHECK(!onePath.append(test->fs, node, "foo").failed());
	CHECK(!otherPath.copy(onePath).failed());

	CHECK(!onePath.append(test->fs, node, "bar").failed());

	CHECK(!otherPath.load(test->fs, node).failed());

	const char *start, *end;
	node.getName(start, end);
	CHECK(memcmp(start, test->foo, strlen(test->foo)) == 0);

	otherPath.dropLast();

	CHECK(!onePath.load(test->fs, node).failed());

	node.getName(start, end);
	CHECK(memcmp(start, test->bar, strlen(test->bar)) == 0);
}

