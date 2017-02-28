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

#ifndef METAIMPL_H_
#define METAIMPL_H_

#include "Wtfs.h"

#include <string.h>

template<class Config>
inline void WtfsEcosystem<Config>::Node::getName(const char*&start, const char*&end) {
	start = key.name;
	end = key.name + pet::Str::nLength(key.name, Config::maxFilenameLength);
}

template<class Config>
inline bool WtfsEcosystem<Config>::Node::isDirectory() {
	return !this->hasData();
}

template<class Config>
inline typename WtfsEcosystem<Config>::NodeId
WtfsEcosystem<Config>::Node::getId() {
	return key.id;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::fetchRoot(Node& node)
{
	static const char* emptyString = "";
	node.key.set(emptyString, emptyString, -1u);
	node.key.id = 0;
	node.initialize(false);
	node.fs = this;
	return true;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::fetchChildByName(Node& node, const char* start, const char* end) {
	if(node.key.id == -1u)
		return pet::GenericError::invalidArgumentError();

	if(!end)
		end = start + strlen(start);

	node.key.set(start, end, node.key.id);
	pet::GenericError ret = this->template get(node.key, node);

	if(ret.failed())
		return ret.rethrow();

	if(!ret)
		return pet::GenericError::noSuchEntryError();

	return ret;
}

template<class Config>
struct ParentIndexComparator {
	const MetaIndexKey<Config::maxFilenameLength>& key;
public:
	inline ParentIndexComparator(const MetaIndexKey<Config::maxFilenameLength>& key): key(key) {}

	inline bool greater(const MetaIndexKey<Config::maxFilenameLength>& subject) {
		return subject.parentId > key.parentId;
	}

	inline bool matches(const MetaIndexKey<Config::maxFilenameLength>& subject) {
		return subject.parentId == key.parentId;
	}
};

template<class Config>
struct ParentKeyComparator {
	const MetaFullKey<Config::maxFilenameLength>& key;
public:
	inline ParentKeyComparator(const MetaFullKey<Config::maxFilenameLength>& key): key(key) {}

	inline bool greater(const typename WtfsEcosystem<Config>::WtfsMain::Element& subject) {
		return subject.key.indexed.parentId > key.indexed.parentId;
	}

	inline bool matches(const typename WtfsEcosystem<Config>::WtfsMain::Element& subject) {
		return subject.key.indexed.parentId == key.indexed.parentId;
	}
};

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::fetchFirstChild(Node& node)
{
	if(node.key.id == -1u)
		return pet::GenericError::invalidArgumentError();

	if(node.hasData())
		return pet::GenericError::isNotDirectoryError();

	node.key.indexed.parentId = node.key.id;
	return this->template search<ParentIndexComparator<Config>, ParentKeyComparator<Config> >(node.key, node);
}

template<class Config>
pet::GenericError
inline WtfsEcosystem<Config>::WtfsMain::fetchById(Node& node, NodeId parent, NodeId id)
{
	if(node.key.id == -1u)
		return pet::GenericError::invalidArgumentError();

	typedef typename MetaTree::Element Element;
	struct MatchHandler {
		inline bool onMatch(Element &e, FullKey& k, FileTree &v) {
			if(e.key.id == k.id) {
				k = e.key;

				if(&v != 0)
					v = e.value;

				return false;
			}

			return true;
		}
	};

	node.key.indexed.parentId = parent;
	node.key.id = id;

	MatchHandler handler;
	pet::GenericError ret = this->template search<ParentIndexComparator<Config>, ParentKeyComparator<Config>, MatchHandler>(node.key, node, handler);

	if(ret.failed())
		return ret.rethrow();

	if(!ret)
		return pet::GenericError::noSuchEntryError();

	return ret;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::fetchChildById(Node& node, NodeId id)
{
	return fetchById(node, node.key.id, id);
}


template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::fetchNextSibling(Node& node)
{
	if(node.key.id == -1u)
		return pet::GenericError::invalidArgumentError();

	struct NextSiblingComparator: ParentKeyComparator<Config> {
	public:
		inline NextSiblingComparator(const FullKey& key): ParentKeyComparator<Config>(key) {}

		inline bool matches(const typename WtfsMain::Element& subject) {
			return ParentKeyComparator<Config>::matches(subject) && subject.key > this->key;
		}
	};

	return this->template search<ParentIndexComparator<Config>, NextSiblingComparator>(node.key, node);
}

template<class Config>
template<bool isFile>
pet::GenericError
inline WtfsEcosystem<Config>::WtfsMain::addNew(Node& node, const char* start, const char* end)
{
	if(node.key.id == -1u)
		return pet::GenericError::invalidArgumentError();

	if(node.hasData())
			return pet::GenericError::isNotDirectoryError();

	if(isReadonly)
		return pet::GenericError::readOnlyFsError();

	node.key.set(start, end, node.key.id);

	maxIdLock.lock();
	node.key.id = maxId;
	node.initialize(isFile);

	pet::GenericError ret = this->insert(node.key, node);

	if(ret.failed()) {
		maxIdLock.unlock();
		return ret.rethrow();
	}

	if(ret) {
		maxId++;
		maxIdLock.unlock();
	} else {
		maxIdLock.unlock();
		return pet::GenericError::alreadyExistsError();
	}

	return ret;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::newDirectory(Node& node, const char* start, const char* end) {
	return addNew<false>(node, start, end);
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::newFile(Node& node, const char* start, const char* end) {
	return addNew<true>(node, start, end);
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::removeNode(Node& node)
{
	if(node.key.id == 0 || node.key.id == -1u)
		return pet::GenericError::invalidArgumentError();

	if(isReadonly)
		return pet::GenericError::readOnlyFsError();

	if(node.hasData()) {
		pet::GenericError ret = node.dispose();

		if(ret.failed())
			return ret.rethrow();
	} else {
		NodeId current = node.key.id;
		NodeId parentId = node.key.indexed.parentId;

		pet::GenericError ret = fetchFirstChild(node);

		if(ret.failed())
			return ret.rethrow();

		if(ret)
			return pet::GenericError::notEmptyError();

		pet::GenericError ret2 = fetchById(node, parentId, current);

		if(ret2.failed())
			return ret2.rethrow();
	}

	pet::GenericError ret = this->remove(node.key, 0);

	if(ret.failed())
		return ret.rethrow();

	if(!ret)
		return pet::GenericError::noSuchEntryError();

	return ret;
}
#endif /* METAIMPL_H_ */
