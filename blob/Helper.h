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

#ifndef HELPER_H_
#define HELPER_H_

#include <cstdint>

#include <limits.h>

#include "meta/ExpLog.h"
#include "meta/Range.h"

template<class Storage>
struct LevelsHelper {
	static constexpr uint32_t base = Storage::pageSize/sizeof(typename Storage::Address);

	template<unsigned int n>
	using logBase = typename pet::log<base>::template x<n>;

	template<unsigned int n>
	using expBase = typename pet::exp<base>::template x<n>;

	static constexpr auto nLevels = logBase<UINT_MAX-1>::value;
	static constexpr auto &sizes = pet::applyOverRange<LevelsHelper<Storage>::expBase, 1, nLevels>::value;

	static inline uint32_t getHighestLevel(uint32_t pageIndex)
	{
		uint32_t ret = 0;

		if(pageIndex == 0)
			return -1;

		while(pageIndex >= sizes[ret]) {
			ret++;
			BlobTreeTrace::assert(ret < nLevels);
		}

		return ret;
	}

	static inline uint32_t getLevelOffset(uint32_t pageIndex, uint32_t level)
	{
		BlobTreeTrace::assert(level != -1u);

		if(level == 0)
			return pageIndex % base;

		return (pageIndex / sizes[level-1]) % base;
	}
};

#endif /* HELPER_H_ */
