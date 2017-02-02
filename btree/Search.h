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

#ifndef SEARCH_H_
#define SEARCH_H_

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template <class IndexComparator, class KeyComparator, class MatchHandler>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>::
checkTable(ROSession &session, const typename Storage::Address& address, Key &key, Value &value, MatchHandler &matchHandler){
	void *ret = this->read(session, address);

	if(!ret)
		return ubiq::GenericError::readError();

	Table* table = (Table*) ret;

	algorithm::Bisect::Result position = algorithm::Bisect::
			find<Element, Key, KeyComparator>(table->elements, table->length(), key);

	if (position.present()) {
		for(int32_t i=position.first(); i <= position.last(); i++){
			if(!matchHandler.onMatch(table->elements[i], key, value)){
				this->release(session, table);
				return true;
			}
		}
	}
	this->release(session, table);
	return false;
}


template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template <class IndexComparator, class KeyComparator, class MatchHandler>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::search(Key &key, Value &value, MatchHandler &matchHandler)
{

	ROSession session(this);

	if(!levels){
		if(root != InvalidAddress) {
			ubiq::GenericError ret = checkTable<IndexComparator, KeyComparator, MatchHandler>(session, root, key, value, matchHandler);
			this->closeReadOnlySession(session);
			return ret;
		}
	}else{
		IndexKey indexKey(key);
		BTree::Iterator iterator(indexKey);
		if(iterate<IndexComparator>(session, iterator).failed()) {
			this->closeReadOnlySession(session);
			return ubiq::GenericError::outOfMemoryError();
		}

		while(iterator.currentAddress != Storage::InvalidAddress) {
			ubiq::GenericError ret = checkTable<IndexComparator, KeyComparator, MatchHandler>(session, iterator.currentAddress, key, value, matchHandler);

			if(ret.failed()) {
				this->closeReadOnlySession(session);
				return ret.rethrow();
			}

			if(ret) {
				this->closeReadOnlySession(session);
				return true;
			}

			if(step<IndexComparator>(session, iterator).failed()) {
				this->closeReadOnlySession(session);
				return ubiq::GenericError::outOfMemoryError();
			}
		}
	}

	this->closeReadOnlySession(session);
	return false;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template <class IndexComparator, class KeyComparator, class MatchHandler>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::search(Key &key, Value &value)
{
	MatchHandler matchHandler;
	return this->search<IndexComparator, KeyComparator, MatchHandler>(key, value, matchHandler);
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::get(Key &key, Value &value)
{
	return search<FullComparator, algorithm::Bisect::DefaultComparator<Element, Key>>(key, value);
}

#endif /* SEARCH_H_ */
