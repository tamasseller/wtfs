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

#ifndef KVTESTKEY_H_
#define KVTESTKEY_H_

#include "algorithm/Fnv.h"

#include <cstring>

struct KVTestKey {
	static const KVTestKey InvalidKey;

	const char* name;

	inline bool operator >(const KVTestKey& than) const
	{
		if(strlen(name) > strlen(than.name))
			return true;

		if(strlen(name) < strlen(than.name))
			return false;

		unsigned int hash = algorithm::Fnv::hash(name);
		unsigned int otherHash = algorithm::Fnv::hash(than.name);

		if(hash > otherHash)
			return true;

		if(hash < otherHash)
			return false;

		return strcmp(name, than.name) > 0;
	}

	inline bool operator ==(const KVTestKey& to) const
	{
		if(!name)
			return to.name == NULL;

		if(!to.name)
			return false;

		return (strlen(name) == strlen(to.name)) && (strcmp(name, to.name) == 0);
	}

	inline KVTestKey(const char* name): name(name){}

private:
	/**
	 * Private constructor for the invalidKey
	 */
	KVTestKey(): name(NULL){}
};

struct KVTestIndexKey {
	unsigned int hash;
	unsigned int length;

	KVTestIndexKey(const KVTestKey &key): hash(algorithm::Fnv::hash(key.name)), length(strlen(key.name)){}

	inline bool operator >(const KVTestIndexKey& than) const
	{
		if(length > than.length)
			return true;

		if(length < than.length)
			return false;

		if(hash > than.hash)
			return true;

		return false;
	}

	inline bool operator ==(const KVTestIndexKey& to) const {
		return (length == to.length) && (hash == to.hash);
	}

};


#endif /* KVTESTKEY_H_ */
