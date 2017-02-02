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

#ifndef BTREEADD_H_
#define BTREEADD_H_

/************************************************/
/* hash = H, newAddress = N, updatedAddress = U */
/* example: idx = 0                             */
/*                                              */
/*     a b             *H* a  b                 */
/*   /  |  \    ->    /   |  | \                */
/* *x*  y   z       *U*  *N* y  z               */
/*                                              */
/* example: idx = 1                             */
/*                                              */
/*     a b              a *H* c                 */
/*   /  |  \    ->    /  |   |  \               */
/*  x  *y*  z        x  *U* *N*  z              */
/*                                              */
/* example: idx = 2                             */
/*                                              */
/*     a b              a b  *H*                */
/*   /  |  \    ->    /  |  |   \               */
/*  x   y  *z*       x   y *U*   *N*            */
/*                                              */
/************************************************/

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
inline typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress BTree<Storage, Key, IndexKey, Value, Allocator>
::splitEntry(RWSession &session, Locator &location, IndexKey hash, Address updatedAddress, Address newAddress) {
	uint32_t level = 0;
	do {
		void *ret = this->read(session, location.current()->address);

		if(!ret)
			return InvalidAddress;

		Node* node = (Node*) ret;
		BtreeTrace::assert(node->numBranches <= node->maxBranches);

		if (node->numBranches < Node::maxBranches) {
			uint32_t insIdx = location.current()->idx;
			node->insert(insIdx, hash, updatedAddress, newAddress);

			if(!location.hasMore())
				this->flagNextAsRoot(session);

			updatedAddress = this->Storage::write(session, node);
			if(updatedAddress == Storage::InvalidAddress)
				return Storage::InvalidAddress;

			newAddress = InvalidAddress;

			break;
		}

		ret = this->empty(session, ++level);

		if(!ret) {
			this->release(session, node);
			return InvalidAddress;
		}

		Node* newNode = (Node *)ret;

		if (location.current()->idx == Node::splitPoint32_t - 1){
			newNode->numBranches = Node::maxBranches - Node::splitPoint32_t + 1;
			node->numBranches = Node::splitPoint32_t;

			node->children[Node::splitPoint32_t-1] = updatedAddress;
			newNode->children[0] = newAddress;

			for(uint32_t i=0; i < (Node::maxBranches - Node::splitPoint32_t); i++){
				newNode->children[i+1] = node->children[i+Node::splitPoint32_t];
				newNode->values[i] = node->values[i+Node::splitPoint32_t-1];
			}

		} else if (location.current()->idx < Node::splitPoint32_t) {
			newNode->numBranches = Node::maxBranches - Node::splitPoint32_t + 1;
			node->numBranches = Node::splitPoint32_t-1;

			for (uint32_t i = 0; i < newNode->numBranches-1; i++)
				newNode->values[i] = node->values[i + Node::splitPoint32_t - 1];

			for (uint32_t i = 0; i < newNode->numBranches; i++)
				newNode->children[i] = node->children[i + Node::splitPoint32_t - 1];

			IndexKey newHash(node->values[Node::splitPoint32_t-2]);

			node->insert(location.current()->idx, hash, updatedAddress, newAddress);

			hash = newHash;
		} else {
			IndexKey newHash(node->values[Node::splitPoint32_t - 1]);

			node->numBranches = Node::splitPoint32_t;
			newNode->numBranches = Node::maxBranches - Node::splitPoint32_t + 1;
			uint32_t insIdx = location.current()->idx - Node::splitPoint32_t;

			for (uint32_t i = 0; i < insIdx; i++)
				newNode->values[i] = node->values[i + Node::splitPoint32_t];

			newNode->values[insIdx] = hash;

			for (uint32_t i = insIdx + 1; i < newNode->numBranches - 1; i++)
				newNode->values[i] = node->values[i + Node::splitPoint32_t - 1];

			for (uint32_t i = 0; i < insIdx; i++)
				newNode->children[i] = node->children[i + Node::splitPoint32_t];

			newNode->children[insIdx] = updatedAddress;
			newNode->children[insIdx + 1] = newAddress;

			for (uint32_t i = insIdx + 2; i < newNode->numBranches; i++)
				newNode->children[i] = node->children[i + Node::splitPoint32_t - 1];

			hash = newHash;
		}

		newAddress = this->Storage::write(session, newNode);
		if(newAddress == Storage::InvalidAddress) {
			this->release(session, node);
			return Storage::InvalidAddress;
		}

		updatedAddress = this->Storage::write(session, node);
		if(updatedAddress == Storage::InvalidAddress)
			return Storage::InvalidAddress;
	} while (location.release());

	if (newAddress != InvalidAddress) {
		FailAddress ret = createRootNode(session, hash, updatedAddress, newAddress, levels);

		if(ret.failed())
			return ret.rethrow();

		levels++;
		return ret;
	} else {
		return propagateUpdate(session, location, updatedAddress);
	}
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template<bool updateAllowed, bool insertAllowed>
inline ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::write(const Key &key, const Value &value) {
	Table* table = 0;
	algorithm::Bisect::Result position;
	uint32_t length;

	RWSession session(this);

	if (root == InvalidAddress) {
		if(!insertAllowed) {
			this->closeReadWriteSession(session);
			return false;
		}

		BtreeTrace::assert(levels == 0);
		this->upgrade(session);

		void *ret = this->empty(session, 0);
		if(!ret) {
			this->closeReadWriteSession(session);
			return ubiq::GenericError::writeError();
		}

		table = (Table *)ret;
		table->elements[0].set(key, value);
		table->terminate(1);

		this->flagNextAsRoot(session);
		Address newAddress = this->Storage::write(session, table);
		if(newAddress == Storage::InvalidAddress) {
			this->closeReadWriteSession(session);
			return ubiq::GenericError::writeError();
		}

		root = newAddress;
		this->commit(session);
	} else {
		if (!levels) {
			void *ret = this->read(session, root);

			if(!ret) {
				this->closeReadWriteSession(session);
				return ubiq::GenericError::readError();
			}

			table = (Table*) ret;
			length = table->length();
			position = algorithm::Bisect::find(table->elements, length, key);

			if (position.present()) {
				if(updateAllowed){
					BtreeTrace::assert(position.single());
					table->elements[position.first()].value = value;
					this->upgrade(session);

					this->flagNextAsRoot(session);
					Address newAddress = this->Storage::write(session, table);
					if(newAddress == Storage::InvalidAddress) {
						this->closeReadWriteSession(session);
						return ubiq::GenericError::writeError();
					}

					root = newAddress;
					this->commit(session);
				}else{
					this->release(session, table);
					this->closeReadWriteSession(session);
					return false;
				}
			} else {
				if(insertAllowed){
					this->upgrade(session);
					if (length == Table::maxElements) {
						ubiq::FailPointer<Table> ret = splitTable(session, *table, position.insertionIndex(), key, value);

						if(ret.failed()) {
							this->release(session, table);
							this->rollback(session);
							return ubiq::GenericError::writeError();
						}

						Table *newTable = ret;
						IndexKey hash(newTable->elements[0].key);

						Address newTableAddress = this->Storage::write(session, newTable);
						if(newTableAddress == Storage::InvalidAddress) {
							this->release(session, table);
							this->rollback(session);
							return ubiq::GenericError::writeError();
						}

						Address tableNewAddress = this->Storage::write(session, table);
						if(tableNewAddress == Storage::InvalidAddress) {
							this->rollback(session);
							return ubiq::GenericError::writeError();
						}

						FailAddress newRoot = createRootNode(session, hash, tableNewAddress, newTableAddress, levels);

						if(newRoot.failed()) {
							this->rollback(session);
							return ubiq::GenericError::writeError();
						}

						levels++;
						root = newRoot;
					} else {
						table->makeRoom(position.insertionIndex(), length).set(key, value);
						table->terminate(length+1);

						this->flagNextAsRoot(session);
						Address newAddress = this->Storage::write(session, table);
						if(newAddress == Storage::InvalidAddress) {
							this->closeReadWriteSession(session);
							return ubiq::GenericError::writeError();
						}

						root = newAddress;
					}

					this->commit(session);
				}else{
					this->release(session, table);
					this->closeReadWriteSession(session);
					return false;
				}
			}
		} else {
			BtreeTrace::assert(root != InvalidAddress);
			Iterator location(key);

			if(iterate<FullComparator>(session, location).failed()) {
				this->closeReadWriteSession(session);
				return ubiq::GenericError::outOfMemoryError();
			}

			Address place = InvalidAddress;
			while (1) {
				if (table)
					this->release(session, table);

				void *ret = this->read(session, location.currentAddress);

				if(!ret) {
					this->closeReadWriteSession(session);
					return ubiq::GenericError::readError();
				}

				table = (Table*) ret;

				length = table->length();

				position = algorithm::Bisect::find(table->elements, length, key);

				if (position.present()) {
					if(updateAllowed){
						BtreeTrace::assert(position.single());
						table->elements[position.first()].value = value;
						this->upgrade(session);

						Address newAddress = this->Storage::write(session, table);
						if(newAddress == Storage::InvalidAddress) {
							this->closeReadWriteSession(session);
							return ubiq::GenericError::writeError();
						}

						FailAddress newRoot = updateOne(session, location.locator, newAddress);

						if(newRoot.failed()) {
							this->rollback(session);
							return ubiq::GenericError::writeError();
						}

						root = newRoot;
						this->commit(session);
						break;
					}else{
						this->release(session, table);
						this->closeReadWriteSession(session);
						return false;
					}
				}

				if(insertAllowed && (place == InvalidAddress || position.insertionIndex() != 0))
					place = location.currentAddress;

				if (location.hasNext()) {
					if(step<FullComparator>(session, location).failed()) {
						this->release(session, table);
						this->closeReadWriteSession(session);
						return ubiq::GenericError::outOfMemoryError();
					}
				}
				else {
					if(insertAllowed){
						if(place != location.currentAddress){
							if(iterate<FullComparator>(session, location).failed()) {
								this->release(session, table);
								this->closeReadWriteSession(session);
								return ubiq::GenericError::outOfMemoryError();
							}

							while(location.currentAddress != place) {
								if(step<FullComparator>(session, location).failed()) {
									this->release(session, table);
									this->closeReadWriteSession(session);
									return ubiq::GenericError::outOfMemoryError();
								}
							}

							this->release(session, table);

							void *ret = (Table*) this->read(session, location.currentAddress);

							if(!ret) {
								this->closeReadWriteSession(session);
								return ubiq::GenericError::readError();
							}

							table = (Table*) ret;


							length = table->length();
							position = algorithm::Bisect::find(table->elements, length, key);
						}

						if (length == Table::maxElements) {
							this->upgrade(session);
							ubiq::FailPointer<Table> ret = splitTable(session, *table, position.insertionIndex(), key, value);

							if(ret.failed()) {
								this->release(session, table);
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							Table *newTable = ret;
							IndexKey hash(newTable->elements[0].key);

							Address newTableAddress = this->Storage::write(session, newTable);
							if(newTableAddress == Storage::InvalidAddress) {
								this->release(session, table);
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							Address tableNewAddress = this->Storage::write(session, table);
							if(tableNewAddress == Storage::InvalidAddress) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							FailAddress newRoot = splitEntry(session, location.locator, hash, tableNewAddress, newTableAddress);

							if(newRoot.failed()) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							root = newRoot;
						} else {
							table->makeRoom(position.insertionIndex(), length).set(key, value);
							table->terminate(length+1);

							this->upgrade(session);
							Address newAddress = this->Storage::write(session, table);
							if(newAddress == Storage::InvalidAddress) {
								this->closeReadWriteSession(session);
								return ubiq::GenericError::writeError();
							}

							FailAddress newRoot = updateOne(session, location.locator, newAddress);

							if(newRoot.failed()) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							root = newRoot;
						}

						this->commit(session);
						break;
					}else{
						this->release(session, table);
						this->closeReadWriteSession(session);
						return false;
					}
				}
			}
		}
	}

	return true;

}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>::put(const Key &key, const Value &value) {
	return write<true, true>(key, value);
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>::insert(const Key &key, const Value &value) {
	return write<false, true>(key, value);
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>::update(const Key &key, const Value &value) {
	return write<true, false>(key, value);
}


#endif /* BTREEADD_H_ */
