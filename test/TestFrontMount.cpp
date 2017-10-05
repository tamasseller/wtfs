/*
 * Mount.cpp
 *
 *  Created on: 2016.12.10.
 *      Author: tooma
 */

#include "1test/Test.h"

#include "FrontCommon.h"

TEST_GROUP(MountWoGc) {
	using Helpers = GcTestHelpers<256, 4, 10, 4, 2, 2>;
	using Fs = typename Helpers::Fs;
	using NodeStream = typename Helpers::NodeStream;
	using Config = typename Helpers::Config;

	Fs::State initialState;

	TEST_SETUP() {
		MOCK("FlashDriver")::disable();
		Fs fs;
		NodeStream foo(fs, "foo"), bar(fs, "bar"), baz(fs, "baz");
		NodeStream dummy1(fs, "dummy1"), dummy2(fs, "dummy2");
		foo.pokeWrite();
		bar.pokeWrite();
		baz.pokeWrite();

		foo.close();
		bar.close();
		baz.close();

		fs.buffers->flush();

		initialState = fs.gatherState();
	}
};

TEST(MountWoGc, RereadFromAnotherSession) {
	Fs fs(false);

	NodeStream foo(fs, "foo", false), bar(fs, "bar", false), baz(fs, "baz", false);

	CHECK(fs.gatherState().resembles(initialState));

	foo.pokeRead();
	bar.pokeRead();
	baz.pokeRead();
}

TEST(MountWoGc, RereadThenWrite) {
	Fs fs(false);

	NodeStream foo(fs, "foo", false), bar(fs, "bar", false), baz(fs, "baz", false);

	CHECK(fs.gatherState().resembles(initialState));
	foo.pokeAppend(2);
	bar.pokeAppend(3);
	baz.pokeAppend(4);
	foo.pokeRead(3);
	bar.pokeRead(4);
	baz.pokeRead(5);
	bar.pokeWrite();
	baz.pokeWrite();
	foo.pokeWrite();
	baz.pokeRead();
	foo.pokeRead();
	bar.pokeRead();
	baz.close();
	foo.close();
	bar.close();
}

