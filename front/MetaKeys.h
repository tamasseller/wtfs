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

#ifndef METAKEYS_H_
#define METAKEYS_H_

#include <cstdint>

#include "algorithm/Fnv.h"
#include "algorithm/Str.h"

template<uint32_t maxFilenameLength>
struct MetaFullKey;

template<uint32_t maxFilenameLength>
struct MetaIndexKey {
	uint32_t parentId = 0;
	uint32_t hash = 0;

	inline MetaIndexKey(const MetaFullKey<maxFilenameLength>& k);
	inline MetaIndexKey() = default;

	inline bool operator >(const MetaIndexKey& than) const;
	inline bool operator ==(const MetaIndexKey& to) const;
};

template<uint32_t maxFilenameLength>
struct MetaFullKey {
	static const MetaFullKey InvalidKey;

	MetaIndexKey<maxFilenameLength> indexed;
	uint32_t id = -1;
	char name[maxFilenameLength + 1] = {'\0',};

	inline void set(const char* n, const char* end);
	inline void set(const char* n, const char* end, uint32_t pid);
	inline MetaFullKey& operator =(const MetaFullKey& other);
	inline bool operator >(const MetaFullKey& than) const;
	inline bool operator ==(const MetaFullKey& to) const;

	inline MetaFullKey() = default;
private:
	inline MetaFullKey(uint32_t pId) {indexed.parentId = pId;}
};

/////////////////////////////////////////////////////////////////////////////////

template<uint32_t maxFilenameLength>
inline MetaIndexKey<maxFilenameLength>::MetaIndexKey (const MetaFullKey<maxFilenameLength>& k): MetaIndexKey(k.indexed) {}

template<uint32_t maxFilenameLength>
inline bool MetaIndexKey<maxFilenameLength>::operator >(const MetaIndexKey<maxFilenameLength>& than) const {
	if(parentId == than.parentId)
		return hash > than.hash;
	else
		return parentId > than.parentId;
}

template<uint32_t maxFilenameLength>
inline bool MetaIndexKey<maxFilenameLength>::operator ==(const MetaIndexKey<maxFilenameLength>& to) const {
	return (parentId == to.parentId) && (hash == to.hash);
}

template<uint32_t maxFilenameLength>
inline void MetaFullKey<maxFilenameLength>::set(const char* n, const char* end)
{
	indexed.hash = pet::Fnv::hash(n, end);

	for(uint32_t i=0; i<maxFilenameLength && n+i < end; i++)
		name[i] = n[i];

	uint32_t l = end-n;
	if(l < maxFilenameLength)
		name[l] = '\0';
}

template<uint32_t maxFilenameLength>
inline void MetaFullKey<maxFilenameLength>::set(const char* n, const char* end, uint32_t pid)
{
	indexed.parentId = pid;
	set(n, end);
}

template<uint32_t maxFilenameLength>
inline MetaFullKey<maxFilenameLength>& MetaFullKey<maxFilenameLength>::operator =(const MetaFullKey<maxFilenameLength>& other) {
	indexed = other.indexed;
	id = other.id;
	pet::Str::ncpy(name, other.name, (uint32_t)sizeof(name));
	return *this;
}

template<uint32_t maxFilenameLength>
inline bool MetaFullKey<maxFilenameLength>::operator >(const MetaFullKey<maxFilenameLength>& than) const {
	if(indexed == than.indexed)
		return pet::Str::ncmp(name, than.name, maxFilenameLength) > 0;
	else
		return indexed > than.indexed;
}

template<uint32_t maxFilenameLength>
inline bool MetaFullKey<maxFilenameLength>::operator ==(const MetaFullKey<maxFilenameLength>& to) const {
	if (indexed == to.indexed)
		return pet::Str::ncmp(name, to.name, maxFilenameLength) == 0;
	else
		return false;
}

template<uint32_t maxFilenameLength>
const MetaFullKey<maxFilenameLength> MetaFullKey<maxFilenameLength>::InvalidKey;

#endif /* METAKEYS_H_ */
