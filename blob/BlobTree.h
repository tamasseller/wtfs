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

#ifndef FILETREE_H_
#define FILETREE_H_

#include <cstdint>

#include "ubiquitous/Trace.h"

class BlobTreeTrace: public ubiq::Trace<BlobTreeTrace> {};

#include "Helper.h"
#include "ubiquitous/Error.h"
#include "pool/Stack.h"

template<class Storage, class Allocator, uint32_t predLevelCount = LevelsHelper<Storage>::nLevels / 2 + 1>
class BlobTree: protected Storage {
	typedef typename Storage::Address Address;
	typedef typename Storage::ReadOnlySession ROSession;
	typedef typename Storage::ReadWriteSession RWSession;
	typedef LevelsHelper<Storage> BlackMagic;
protected:
	Address root;
	uint32_t size;

	template<class Callback>
	ubiq::GenericError traverse(RWSession &session, Callback &&);
public:
	inline BlobTree(Address fileRoot, uint32_t size);

	ubiq::FailPointer<void> empty();
	ubiq::FailPointer<void> read(uint32_t page);
	ubiq::GenericError update(uint32_t page, uint32_t newSize, void*);
	void release(void*);

	uint32_t getSize();

	struct State {
		Address address;
		uint32_t idx;
		uint32_t maxIdx;
		bool lastEntryOnLevel;
	};

	typedef mm::DynamicStack<State, Allocator, predLevelCount> Traversor;

	ubiq::GenericError dispose();
	ubiq::GenericError relocate(Address &page);
};

#include "TreeOperations.h"

#endif /* FILETREE_H_ */
