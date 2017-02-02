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

#ifndef SIMPLEKEY_H_
#define SIMPLEKEY_H_

struct Key {
	static const Key InvalidKey;
	int value;

	inline bool operator >(const Key& than) const {
		return value > than.value;
	}

	inline bool operator ==(const Key& to) const {
		return value == to.value;
	}

	inline operator int () const{
		return value;
	}

	inline Key(unsigned int value): value(value) {}
};

#endif /* SIMPLEKEY_H_ */
