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

#include "FrontCommon.h"

TEST_GROUP(GcBlob) {
	using Helpers = GcTestHelpers<256, 4, 7, 3, 1, 2>;
	using Fs = typename Helpers::Fs;
	using NodeStream = typename Helpers::NodeStream;
	using Config = typename Helpers::Config;
	Fs fs;

	TEST_SETUP() {
		mock().disable();
	}
};

/*
 * This test aims to trigger a content level gc event.
 *
 * It takes many steps to create a content level block layout like this:
 *
 * Note: names in (parentheses) are garbage of named file.
 *
 *   +-----------------------------+
 *   | Block A                     |
 *   |                             |
 *   |  foo (bar) (bar) ... (bar)  |
 *   |                             |
 *   *-----------------------------+
 *
 *   +-----------------------------+
 *   | Block B                     |
 *   |                             |
 *   |  bar (baz) (baz) ... baz    |
 *   |                             |
 *   *-----------------------------+
 *
 * Also at this stage there are no more free unassigned blocks left,
 * so a GC is triggered by the last write of baz. After the GC Block A and B
 * should be emptied out and somewhere has to be a Block C like this:
 *
 *   +-----------------------------+
 *   | Block C                     |
 *   |                             |
 *   |  foo bar baz - - .... -     |
 *   |                             |
 *   *-----------------------------+
 *
 * Note: The order of foo and bar can differ.
 *
 * The exact layout or that fact that the GC is actually triggered
 * is not checked by this test, only that the files still contain the
 * expected values.
 *
 * It can be verified by coverage analysis that the GC is actually triggered.
 */

TEST(GcBlob, ContentLevelGcTriggerSimple) {
	NodeStream foo(fs, "foo"), bar(fs, "bar"), baz(fs, "baz");

	/* Put foo in block A */
	foo.pokeWrite();

	/* Fill the rest of Block A */
	for(unsigned int i=0; i<Config::FlashDriver::blockSize - 1; i++)
		bar.pokeWrite();

	/* Start Block B */
	bar.pokeWrite();

	/* Finish Block B */
	for(unsigned int i=0; i<Config::FlashDriver::blockSize - 2; i++)
		baz.pokeWrite();

	/* Trigger GC */
	baz.pokeWrite();

	/* Back check */
	foo.pokeRead();
	bar.pokeRead();
	baz.pokeRead();
}

/*
 *   +-----------------------------+
 *   | Block A                     |
 *   |                             |
 *   |  (foo) (foo) ... foo bar1   |
 *   |                             |
 *   *-----------------------------+
 *
 *   +-----------------------------+
 *   | Block B                     |
 *   |                             |
 *   |  bar2 (baz) (baz) ... baz   |
 *   |                             |
 *   *-----------------------------+
 *
 *   +-----------------------------+
 *   | Block C                     |
 *   |                             |
 *   |  foo bar1 - - .... -        |
 *   |                             |
 *   *-----------------------------+
 */
TEST(GcBlob, ContentLevelGcTriggerPropagation) {
	NodeStream foo(fs, "foo"), bar(fs, "bar"), baz(fs, "baz");
	const unsigned int times = Config::FlashDriver::pageSize / (strlen(foo.name) + 1) + 1;

	/* Put foo in block A */
	for(unsigned int i=0; i<Config::FlashDriver::blockSize-1; i++)
		foo.pokeWrite();

	/* Start Block B */
	bar.pokeWrite(times);

	/* Finish Block B */
	for(unsigned int i=0; i<Config::FlashDriver::blockSize-2; i++)
		baz.pokeWrite();

	/* Trigger GC */
	baz.pokeWrite();

	/* Back check */
	foo.pokeRead();
	bar.pokeRead(times);
	baz.pokeRead();
}

TEST(GcBlob, MultiContentLevelGcTrigger) {
	NodeStream foo(fs, "foo"), bar(fs, "bar"), baz(fs, "baz");

	fs.buffers->flush();

	for(unsigned int i=0; i<Config::FlashDriver::blockSize-1; i++)
		foo.pokeWrite();

	bar.pokeWrite();

	for(unsigned int i=0; i<Config::FlashDriver::blockSize-1; i++)
		baz.pokeWrite();

	foo.pokeWrite();

	baz.close();

	for(unsigned int i=0; i<Config::FlashDriver::blockSize-2; i++)
		bar.pokeWrite();

	foo.pokeWrite();

	foo.pokeRead();
	bar.pokeRead();
	baz.open(fs, "baz");
	baz.pokeRead();
}

TEST_GROUP(GcMeta) {
	using Helpers = GcTestHelpers<256, 4, 5, 3, 2, 0>;
	using Fs = typename Helpers::Fs;
	Fs fs;

	TEST_SETUP() {
		mock().disable();
	}
};

TEST(GcMeta, MetaLevelGcTrigger) {
	Fs::Node node;
	Helpers::createFile(fs, node, "dummy1");
	Helpers::createFile(fs, node, "dummy2");
	Helpers::createFile(fs, node, "dummy3");
	Helpers::createFile(fs, node, "dummy4");
	Helpers::createFile(fs, node, "dummy5");

	Helpers::removeFile(fs, node, "dummy1");
	Helpers::createFile(fs, node, "dummy1");

	Helpers::removeFile(fs, node, "dummy1");
	Helpers::createFile(fs, node, "dummy1");
}

TEST_GROUP(GcCombined) {
	using Helpers = GcTestHelpers<256, 4, 9, 3, 2, 2>;
	using Fs = typename Helpers::Fs;
	using NodeStream = typename Helpers::NodeStream;
	using Config = typename Helpers::Config;

	Fs fs;

	TEST_SETUP() {
		mock().disable();
	}
};

TEST(GcCombined, MultiplesLevelsGcTrigger) {
	Fs::Node node;
	Helpers::createFile(fs, node, "dummy1");
	Helpers::createFile(fs, node, "dummy2");
	Helpers::createFile(fs, node, "dummy3");

	NodeStream foo(fs, "foo");
	fs.buffers->flush();

	NodeStream bar(fs, "bar");
	fs.buffers->flush();

	NodeStream baz(fs, "baz");
	fs.buffers->flush();

	const unsigned int times = Config::FlashDriver::pageSize / (strlen(foo.name) + 1) + 1;

	for(unsigned int i=0; i<10; i++) {
		for(unsigned int j=0; j<10; j++) {
			for(unsigned int k=0; k<10; k++) {
				foo.pokeWrite(times);
			}

			bar.pokeWrite();
		}

		baz.pokeWrite();
	}

	foo.pokeRead(times);
	bar.pokeRead(times);
	baz.pokeRead(times);
}
