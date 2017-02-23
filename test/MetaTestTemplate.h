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

#ifndef TESTTEMPLATES_H_
#define TESTTEMPLATES_H_

#include "ubiquitous/Error.h"
#include "pet/test/MockAllocator.h"

#include <string.h>

template <class Fs>
struct FsMetaTestTemplate {
	static void requireSuccess(pet::GenericError status) {
		CHECK(!status.failed());
		CHECK(status);
	}

	static void requireFailure(pet::GenericError status) {
		CHECK(!status.failed());
		CHECK(!status);
	}

	template<class Error>
	static void requireError(pet::GenericError status, Error error) {
		CHECK(status.failed());
		CHECK(status == error);
	}

	struct MetaEmptyTree {
		Fs fs;

		inline void uninitializedNodeInvalidArgumentFetchChildById() {
			typename Fs::Node node;
			requireError(fs.fetchChildById(node, 2), pet::GenericError::invalidArgument);
		}

		inline void uninitializedNodeInvalidArgumentFetchByName() {
			typename Fs::Node node;
			const char *name = "asd";
			requireError(fs.fetchChildByName(node, name, name + strlen(name)), pet::GenericError::invalidArgument);
		}

		inline void uninitializedNodeInvalidArgumentFetchFirst() {
			typename Fs::Node node;
			requireError(fs.fetchFirstChild(node), pet::GenericError::invalidArgument);
		}

		inline void uninitializedNodeInvalidArgumentFetchNext() {
			typename Fs::Node node;
			requireError(fs.fetchNextSibling(node), pet::GenericError::invalidArgument);
		}

		inline void uninitializedNodeInvalidArgumentRemove() {
			typename Fs::Node node;
			requireError(fs.removeNode(node), pet::GenericError::invalidArgument);
		}

		inline void uninitializedNodeInvalidArgumentAdd() {
			typename Fs::Node node;
			requireError(fs.fetchFirstChild(node), pet::GenericError::invalidArgument);
		}

		inline void noSuchId() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireError(fs.fetchChildById(node, 2), pet::GenericError::noSuchEntry);
		}

		inline void noSuchName() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *name = "asd";
			requireError(fs.fetchChildByName(node, name, name + strlen(name)), pet::GenericError::noSuchEntry);
		}

		inline void removeRoot() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireError(fs.removeNode(node), pet::GenericError::invalidArgument);
		}

		inline void noChild() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireFailure(fs.fetchFirstChild(node));
		}

		inline void addFile() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *name = "foo";
			requireSuccess(fs.newFile(node, name, name + strlen(name)));
		}


		inline void failToAddFileAgain() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *name = "foo";
			requireSuccess(fs.newFile(node, name, name + strlen(name)));

			typename Fs::Node nodeAgain;
			requireSuccess(fs.fetchRoot(nodeAgain));
			requireError(fs.newFile(nodeAgain, name, name + strlen(name)), pet::GenericError::alreadyExists);
		}

		inline void addAnotherFile() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *name = "foo";
			requireSuccess(fs.newFile(node, name, name + strlen(name)));

			typename Fs::Node anotherNode;
			requireSuccess(fs.fetchRoot(anotherNode));
			const char *anotherName = "bar";
			requireSuccess(fs.newFile(anotherNode, anotherName, anotherName + strlen(anotherName)));
		}

		inline void addDir() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *name = "fooDir";
			requireSuccess(fs.newDirectory(node, name, name + strlen(name)));
		}

		inline void addIntoDir() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *dirName = "fooDir";
			requireSuccess(fs.newDirectory(node, dirName, dirName + strlen(dirName)));

			const char *fileName = "fooFile";
			requireSuccess(fs.newFile(node, fileName , fileName + strlen(fileName)));
		}

		inline void addToRootAndToDirWithSameName() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *fileName = "fooFile";
			requireSuccess(fs.newFile(node, fileName , fileName + strlen(fileName)));

			const char *dirName = "fooDir";
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.newDirectory(node, dirName, dirName + strlen(dirName)));

			requireSuccess(fs.newFile(node, fileName , fileName + strlen(fileName)));
		}
	};

	struct MetaFooBarStraight {
		Fs fs;
		typename Fs::NodeId dirId, fileId;
		const char *dirName = "foo";
		const char *fileName = "bar";

		void setup() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.newDirectory(node, dirName, dirName + strlen(dirName)));
			dirId = node.getId();
			requireSuccess(fs.newFile(node, fileName , fileName + strlen(fileName)));
			fileId = node.getId();
		}

		inline void findInRootById() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			const char *start, *end;
			node.getName(start, end);
			STRCMP_EQUAL(dirName, start);
		}

		inline void isDir() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			CHECK(node.isDirectory());
			requireSuccess(fs.fetchChildById(node, fileId));
			CHECK(!node.isDirectory());
		}

		inline void findInvalid() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			requireError(fs.fetchChildById(node, dirId), pet::GenericError::noSuchEntry);
		}

		inline void findInSubdirById() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			requireSuccess(fs.fetchChildById(node, fileId));
			const char *start, *end;
			node.getName(start, end);
			STRCMP_EQUAL(fileName, start);
		}

		inline void findInRootByName() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildByName(node, dirName, dirName + strlen(dirName)));
			const char *start, *end;
			node.getName(start, end);
			STRCMP_EQUAL(dirName, start);
		}

		inline void findInSubdirByName() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildByName(node, dirName, dirName + strlen(dirName)));
			requireSuccess(fs.fetchChildByName(node, fileName, fileName + strlen(fileName)));
			const char *start, *end;
			node.getName(start, end);
			STRCMP_EQUAL(fileName, start);
		}

		inline void addAnotherFileInSubdir() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildByName(node, dirName, dirName + strlen(dirName)));
			const char *newName = "foobar";
			requireSuccess(fs.newFile(node, newName, newName + strlen(newName)));
		}

		inline void failNonDirectIdFetch() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireError(fs.fetchChildById(node, fileId), pet::GenericError::noSuchEntry);
		}

		inline void failToDeleteFoo() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			pet::GenericError ret = fs.removeNode(node);

			CHECK(ret.failed());
			CHECK(ret == pet::GenericError::notEmpty);
		}

		inline void deleteBar() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			requireSuccess(fs.fetchChildById(node, fileId));
			requireSuccess(fs.removeNode(node));
		}

		inline void deleteBarThanFoo() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			requireSuccess(fs.fetchChildById(node, fileId));
			requireSuccess(fs.removeNode(node));

			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.fetchChildById(node, dirId));
			requireSuccess(fs.removeNode(node));
		}
	};

	struct MetaFooBarFlat {
		Fs fs;
		typename Fs::NodeId rootId, fooId, barId;
		const char *fooName = "foo";
		const char *barName = "bar";

		void setup() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			rootId = node.getId();
			requireSuccess(fs.newFile(node, fooName, fooName + strlen(fooName)));
			fooId = node.getId();
			requireSuccess(fs.fetchRoot(node));
			requireSuccess(fs.newFile(node, barName, barName + strlen(barName)));
			barId = node.getId();
		}

		inline void traverse() {
			typename Fs::Node node;
			requireSuccess(fs.fetchRoot(node));
			const char *start, *unused;
			requireSuccess(fs.fetchFirstChild(node));

			node.getName(start, unused);

			if(!start)
				return;

			bool firstWasFoo = (strcmp(start, fooName) == 0);
			bool firstWasBar = (strcmp(start, barName) == 0);
			CHECK(firstWasFoo || firstWasBar);

			requireSuccess(fs.fetchNextSibling(node));
			node.getName(start, unused);

			bool secondWasFoo = (strcmp(start, fooName) == 0);
			bool secondWasBar = (strcmp(start, barName) == 0);
			CHECK(secondWasFoo || secondWasBar);
			CHECK((firstWasFoo && secondWasBar) || (secondWasFoo && firstWasBar));

			requireFailure(fs.fetchNextSibling(node));
		}

		inline void concurrentRemove() {
			typename Fs::Node node1, node2;
			requireSuccess(fs.fetchRoot(node1));
			requireSuccess(fs.fetchFirstChild(node1));
			requireSuccess(fs.fetchRoot(node2));
			requireSuccess(fs.fetchFirstChild(node2));

			requireSuccess(fs.removeNode(node1));
			requireError(fs.removeNode(node2), pet::GenericError::noSuchEntry);
		}

		inline void concurrentCreate() {
			typename Fs::Node node1, node2;
			requireSuccess(fs.fetchRoot(node1));
			requireSuccess(fs.fetchRoot(node2));
			const char *fooBar = "foobar";
			requireSuccess(fs.newFile(node1, fooBar, fooBar + strlen(fooBar)));
			requireError(fs.newFile(node2, fooBar, fooBar + strlen(fooBar)), pet::GenericError::alreadyExists);
		}
	};
};

#define FS_META_TEST_TEMPLATE(x)   																									\
TEST_GROUP(MetaEmptyTree ## x) {typename FsMetaTestTemplate<x>::MetaEmptyTree test; };												\
TEST(MetaEmptyTree ## x, UninitializedNodeInvalidArgumentFetchChildById) {test.uninitializedNodeInvalidArgumentFetchChildById();}	\
TEST(MetaEmptyTree ## x, UninitializedNodeInvalidArgumentFetchByName) {test.uninitializedNodeInvalidArgumentFetchByName();}			\
TEST(MetaEmptyTree ## x, UninitializedNodeInvalidArgumentFetchFirst) {test.uninitializedNodeInvalidArgumentFetchFirst();}			\
TEST(MetaEmptyTree ## x, UninitializedNodeInvalidArgumentFetchNext) {test.uninitializedNodeInvalidArgumentFetchNext();}				\
TEST(MetaEmptyTree ## x, UninitializedNodeInvalidArgumentRemove) {test.uninitializedNodeInvalidArgumentRemove();}					\
TEST(MetaEmptyTree ## x, UninitializedNodeInvalidArgumentAdd) {test.uninitializedNodeInvalidArgumentAdd();}							\
TEST(MetaEmptyTree ## x, NoSuchId) {test.noSuchId();}   														    				\
TEST(MetaEmptyTree ## x, NoSuchName) {test.noSuchName();}   													    				\
TEST(MetaEmptyTree ## x, RemoveRoot) {test.removeRoot();}																			\
TEST(MetaEmptyTree ## x, NoChild) {test.noChild();}   														    					\
TEST(MetaEmptyTree ## x, AddFile) {test.addFile();}   														    					\
TEST(MetaEmptyTree ## x, FailToAddFileAgain) {test.failToAddFileAgain();}   									    				\
TEST(MetaEmptyTree ## x, AddAnotherFile) {test.addAnotherFile();}   											    				\
TEST(MetaEmptyTree ## x, AddDir) {test.addDir();}   															    				\
TEST(MetaEmptyTree ## x, AddIntoDir) {test.addIntoDir();}   													    				\
TEST(MetaEmptyTree ## x, AddToRootAndToDirWithSameName) {test.addToRootAndToDirWithSameName();}   			    					\
TEST_GROUP(MetaFooBarStraight ## x) {typename FsMetaTestTemplate<x>::MetaFooBarStraight test; TEST_SETUP() { test.setup(); }}; 		\
TEST(MetaFooBarStraight ## x, FindInRootById) {test.findInRootById();}   									    					\
TEST(MetaFooBarStraight ## x, IsDir) {test.isDir();}   									    										\
TEST(MetaFooBarStraight ## x, FindInvalid) {test.findInvalid();}																	\
TEST(MetaFooBarStraight ## x, FindInSubdirById) {test.findInSubdirById();}   								    					\
TEST(MetaFooBarStraight ## x, FindInRootByName) {test.findInRootByName();}   								    					\
TEST(MetaFooBarStraight ## x, FindInSubdirByName) {test.findInSubdirByName();}   							   						\
TEST(MetaFooBarStraight ## x, AddAnotherFileInSubdir) {test.addAnotherFileInSubdir();}   					    					\
TEST(MetaFooBarStraight ## x, FailNonDirectIdFetch) {test.failNonDirectIdFetch();}   						    					\
TEST(MetaFooBarStraight ## x, FailToDeleteFoo) {test.failToDeleteFoo();}   									    					\
TEST(MetaFooBarStraight ## x, DeleteBar) {test.deleteBar();}   												   						\
TEST(MetaFooBarStraight ## x, DeleteBarThanFoo) {test.deleteBarThanFoo();}   								    					\
TEST_GROUP(MetaFooBarFlat ## x) {typename FsMetaTestTemplate<x>::MetaFooBarFlat test; TEST_SETUP() { test.setup(); }};   			\
TEST(MetaFooBarFlat ## x, Traverse) {test.traverse();}   													    					\
TEST(MetaFooBarFlat ## x, ConcurrentRemove) {test.concurrentRemove();}   									    					\
TEST(MetaFooBarFlat ## x, ConcurrentCreate) {test.concurrentCreate();}

#endif /* TESTTEMPLATES_H_ */
