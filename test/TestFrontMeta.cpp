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

#include "Wtfs.h"

#include "MetaTestTemplate.h"
#include "MockFlashDriver.h"

struct Config: public DefaultNolockConfig  {
	typedef MockFlashDriver<2112, 64, 64> FlashDriver;
	typedef ::Allocator Allocator;

	static constexpr unsigned int nBuffers = 10;
	static constexpr unsigned int maxMeta = 5;
	static constexpr unsigned int maxFile = 5;
	static constexpr uint32_t maxFilenameLength = 47;
};

struct Fs: public Wtfs<Config> {
	Buffers inlineBuffers;
	inline Fs() {
		bind(&inlineBuffers);
		auto x = initialize(true);
		x.failed(); // Nothing to do about it.
	}
};


FS_META_TEST_TEMPLATE(Fs)
