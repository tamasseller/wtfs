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

#ifndef TEXTVALUE_H_
#define TEXTVALUE_H_

#include <string.h>

struct TextValue {
	char content[33];

public:
	inline TextValue(const char* const &input) {
		strncpy(content, input, (unsigned int) sizeof(content));
	}

	inline operator const char* const () const{
		return content;
	}

	inline bool operator ==(const char* const other) const{
		return strncmp(content, other, (unsigned int) sizeof(content)) == 0;
	}
};

#endif /* TEXTVALUE_H_ */
