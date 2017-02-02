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

#ifndef POORHASHKEY_H_
#define POORHASHKEY_H_

struct PoorHashKey {
	static const PoorHashKey InvalidKey;

	unsigned int key;
	inline bool operator >(const PoorHashKey& than) const {
		if (key > than.key)
			return true;

		return false;
	}

	inline bool operator ==(const PoorHashKey& to) const {
		return key == to.key;
	}

	inline PoorHashKey(unsigned int key): key(key) {}

private:
	/**
	 * Private constructor for the invalidKey
	 */
	PoorHashKey(): key(-1u){}
};

struct PoorHashIndexKey {
	PoorHashIndexKey (const PoorHashKey& k) {}

	inline bool operator >(const PoorHashIndexKey& than) const {
		return false;
	}

	inline bool operator ==(const PoorHashIndexKey& to) const {
		return true;
	}
};

#endif /* POORHASHKEY_H_ */
