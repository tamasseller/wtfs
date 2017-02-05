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

#ifndef DATEKEY_H_
#define DATEKEY_H_

#include "algorithm/Str.h"

struct DateKey {
	static const DateKey InvalidKey;

	/**
	 * These are the primary key
	 */
	unsigned int year;
	unsigned int month;
	unsigned int day;

	/**
	 * This is not part of the primary key. If a field needs to be searched by then
	 * it has to be included in the key, even if it is not part of the primary key.
	 */
	unsigned int eventRating;

	/**
	 * The > and == operators are required to take only the primary key into account.
	 */
	inline bool operator >(const DateKey& than) const {
		if (year > than.year)
			return true;

		if (year < than.year)
			return false;

		if (month > than.month)
			return true;

		if (month < than.month)
			return false;

		if (day > than.day)
			return true;

		return false;
	}

	inline bool operator ==(const DateKey& to) const {
		return year == to.year && month == to.month && day == to.day;
	}

	inline DateKey(unsigned int year, unsigned int month, unsigned int day, unsigned int eventRating=0):
			year(year), month(month), day(day), eventRating(eventRating) {}

private:
	/**
	 * Private constructor for the invalidKey
	 */
	DateKey(): year(0), month(0), day(0), eventRating(0){}
};

/**
 * Index level key, only contains part of the primary key
 */
struct YearKey {
	unsigned int year;

	YearKey (const DateKey& k): year(k.year) {}

	inline bool operator >(const YearKey& than) const {
		return (year > than.year);
	}

	inline bool operator ==(const YearKey& to) const {
		return (year == to.year);
	}
};

#endif /* DATEKEY_H_ */
