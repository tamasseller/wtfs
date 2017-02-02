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

#ifndef BTREEDEL_H_
#define BTREEDEL_H_

/************************************************/
/* newAddress = N                               */
/* example: idx = 0                             */
/*                                              */
/*     a b              b                       */
/*   /  |  \    ->    /   \                     */
/* *x* *y*  z       *N*    z                    */
/*  ^                                           */
/*                                              */
/* example: idx = 1                             */
/*                                              */
/*     a b              a                       */
/*   /  |  \    ->    /   \                     */
/*  x  *y* *z*       x    *N*                   */
/*      ^                                       */
/*                                              */
/************************************************/

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
template<class T>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::actionPlanner(ROSession &session, PlanOfAction<T>& plan, Locator &location){
	LevelLocation &level = *location.current();
	if ((level.smallerSibling != InvalidAddress) && (level.greaterSibling != InvalidAddress)) {
		T *little, *big;
		uint32_t littleLength, bigLength;

		void *ret = this->read(session, level.smallerSibling);

		if(!ret)
			return ubiq::GenericError::readError();

		little = (T*) ret;
		littleLength = little->length();
		if (littleLength == T::splitPoint32_t) {
			plan.action = PlanOfAction<T>::mergeDown;
			plan.partner = little;
			plan.length = littleLength;
		} else {
			void *ret = this->read(session, level.greaterSibling);

			if(!ret) {
				this->release(session, little);
				return ubiq::GenericError::readError();
			}

			big = (T*) ret;
			bigLength = big->length();

			if (bigLength == T::splitPoint32_t) {
				plan.action = PlanOfAction<T>::mergeUp;
				plan.partner = big;
				plan.length = bigLength;
				this->release(session, little);
			} else {
				if (bigLength > littleLength) {
					plan.action = PlanOfAction<T>::redistGreater;
					plan.partner = big;
					plan.length = bigLength;
					this->release(session, little);
				} else {
					plan.action = PlanOfAction<T>::redistSmaller;
					plan.partner = little;
					plan.length = littleLength;
					this->release(session, big);
				}
			}
		}
	} else {
		if (level.smallerSibling != InvalidAddress) {
			void *ret = this->read(session, level.smallerSibling);

			if(!ret)
				return ubiq::GenericError::readError();

			plan.partner = (T*) ret;

			plan.length = plan.partner->length();
			if (plan.length == T::splitPoint32_t)
				plan.action = PlanOfAction<T>::mergeDown;
			else
				plan.action = PlanOfAction<T>::redistSmaller;
		} else {
			BtreeTrace::assert(level.greaterSibling != InvalidAddress);

			void *ret = this->read(session, level.greaterSibling);

			if(!ret)
				return ubiq::GenericError::readError();

			plan.partner = (T*) ret;

			plan.length = plan.partner->length();
			if (plan.length == T::splitPoint32_t)
				plan.action = PlanOfAction<T>::mergeUp;
			else
				plan.action = PlanOfAction<T>::redistGreater;
		}
	}

	return true;
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
typename BTree<Storage, Key, IndexKey, Value, Allocator>::FailAddress BTree<Storage, Key, IndexKey, Value, Allocator>
::mergeEntry(RWSession& session, Locator &location, Address newAddress, MergeDirection direction, bool rootHasTwo)
{
	while (1) {
		uint32_t idl, idh;

		if(direction == MergeDirection::Down){
			BtreeTrace::assert(location.current()->idx > 0);
			idh = location.current()->idx;
			idl = idh - 1;
		}else{
			BtreeTrace::assert(location.current()->idx < Node::maxBranches);
			idl = location.current()->idx;
			idh = idl + 1;
		}

		void *ret = this->read(session, location.current()->address);

		if(!ret)
			return InvalidAddress;

		Node* node = (Node*) ret;

		if(location.hasMore() && node->numBranches == Node::splitPoint32_t) {
			location.release();
			PlanOfAction<Node> plan;

			if(actionPlanner(session, plan, location).failed()) {
				this->release(session, node);
				return InvalidAddress;
			}

			uint32_t amount;
			if(plan.action == PlanOfAction<Node>::mergeUp) {
				node->remove(idl, newAddress);
				node->values[node->numBranches-1] = location.current()->greaterValue;

				for(uint32_t i=0; i<plan.length-1; i++)
					node->values[node->numBranches+i] = plan.partner->values[i];

				for(uint32_t i=0; i<plan.length; i++)
					node->children[node->numBranches+i] = plan.partner->children[i];

				this->disposeBuffered(session, (void*)plan.partner);

				node->numBranches += plan.length;

				if(rootHasTwo && !location.hasMore())
					this->flagNextAsRoot(session);

				newAddress = this->Storage::write(session, node);

				if(newAddress == Storage::InvalidAddress)
					return Storage::InvalidAddress;

				direction = MergeDirection::Up;
			} else if(plan.action == PlanOfAction<Node>::mergeDown) {
				plan.partner->values[plan.length-1] = location.current()->smallerValue;

				for(uint32_t i=0; i<idl; i++)
					plan.partner->values[plan.length+i] = node->values[i];

				for(uint32_t i=idl+1; i<node->numBranches-1; i++)
					plan.partner->values[plan.length+i-1] = node->values[i];

				for(uint32_t i=0; i<idl; i++)
					plan.partner->children[plan.length+i] = node->children[i];

				plan.partner->children[plan.length+idl] = newAddress;

				for(uint32_t i=idl+2; i<node->numBranches; i++)
					plan.partner->children[plan.length+i-1] = node->children[i];

				plan.partner->numBranches += node->numBranches - 1;

				this->disposeBuffered(session, (void*)node);

				if(rootHasTwo && !location.hasMore())
					this->flagNextAsRoot(session);

				newAddress = this->Storage::write(session, plan.partner);

				if(newAddress == Storage::InvalidAddress)
					return Storage::InvalidAddress;

				direction = MergeDirection::Down;
			} else if(plan.action == PlanOfAction<Node>::redistGreater) {
				amount = ((plan.length - Node::splitPoint32_t + 1) / 2);
				BtreeTrace::assert(amount > 0);
				node->remove(idl, newAddress);

				node->values[node->numBranches-1] = location.current()->greaterValue;

				for(uint32_t i=0; i<amount-1; i++)
					node->values[node->numBranches+i] = plan.partner->values[i];

				for(uint32_t i=0; i<amount; i++)
					node->children[node->numBranches+i] = plan.partner->children[i];

				IndexKey divider = IndexKey(plan.partner->values[amount-1]);

				for(uint32_t i=0; i<plan.length-amount; i++) {
					plan.partner->values[i] = plan.partner->values[i+amount];
					plan.partner->children[i] = plan.partner->children[i+amount];
				}

				plan.partner->numBranches -= amount;
				node->numBranches += amount;

				Address newPartnerAddress = this->Storage::write(session, plan.partner);
				if(newPartnerAddress == Storage::InvalidAddress) {
					this->release(session, node);
					return Storage::InvalidAddress;
				}

				Address newNodeAddress = this->Storage::write(session, node);
				if(newNodeAddress == Storage::InvalidAddress) {
					return Storage::InvalidAddress;
				}

				return updateTwo(session, location, UpdateDirection::Up, divider, newNodeAddress, newPartnerAddress);
			} else if(plan.action == PlanOfAction<Node>::redistSmaller) {
				amount = ((plan.length - Node::splitPoint32_t + 1) / 2);
				BtreeTrace::assert(amount > 0);
				node->remove(idl, newAddress); // PERF (ezt lehet másolás közben is)

				for(int32_t i=node->numBranches-2; i >= 0; i--)
					node->values[i+amount] = node->values[i];

				for(int32_t i=node->numBranches-1; i >= 0; i--)
					node->children[i+amount] = node->children[i];

				node->values[amount-1] = location.current()->smallerValue;

				for(uint32_t i=0; i < amount-1; i++)
					node->values[i] = plan.partner->values[plan.length-amount+i];

				for(uint32_t i=0; i < amount; i++)
					node->children[i] = plan.partner->children[plan.length-amount+i];

				IndexKey divider(plan.partner->values[plan.length-amount-1]);

				plan.partner->numBranches -= amount;
				node->numBranches += amount;

				Address newPartnerAddress = this->Storage::write(session, plan.partner);
				if(newPartnerAddress == Storage::InvalidAddress) {
					this->release(session, node);
					return Storage::InvalidAddress;
				}

				Address newNodeAddress = this->Storage::write(session, node);
				if(newNodeAddress == Storage::InvalidAddress)
					return Storage::InvalidAddress;

				return updateTwo(session, location, UpdateDirection::Down, divider, newNodeAddress, newPartnerAddress);
			}
		} else {
			if(node->numBranches == 2){ // drop current root
				BtreeTrace::assert(!location.hasMore());
				this->disposeBuffered(session, (void*)node);

				levels--;
				return newAddress;
			} else {

				BtreeTrace::assert(!location.hasMore() || (node->numBranches > Node::splitPoint32_t));

				node->remove(idl, newAddress);

				if(location.release()) {
					Address newAddress = this->Storage::write(session, node);
					if(newAddress == Storage::InvalidAddress)
						return Storage::InvalidAddress;

					return updateOne(session, location, newAddress);
				} else {
					this->flagNextAsRoot(session);
					return this->Storage::write(session, node);
				}
			}
		}
	}
}

template <class Storage, class Key, class IndexKey, class Value, class Allocator>
ubiq::GenericError BTree<Storage, Key, IndexKey, Value, Allocator>
::remove(const Key &key, Value* returnedValue) {
	Table* table;
	algorithm::Bisect::Result position;
	uint32_t length;

	RWSession session(this);

	if (root == InvalidAddress) {
		this->closeReadWriteSession(session);
		return false;
	}

	if (!levels) {
		void *ret = (Table*) this->read(session, root);

		if(!ret) {
			this->closeReadWriteSession(session);
			return ubiq::GenericError::readError();
		}

		table = (Table*) ret;

		length = table->length();

		position = algorithm::Bisect::find(table->elements, length, key);

		if (!position.present()) {
			this->release(session, table);
			this->closeReadWriteSession(session);
			return false;
		}

		BtreeTrace::assert(position.single());

		if(returnedValue)
			*returnedValue = table->elements[position.first()].value;

		this->upgrade(session);

		if(length == 1){
			this->disposeBuffered(session, (void*)table);
			root = InvalidAddress;
		}else{
			table->remove(position.first(), length);

			this->flagNextAsRoot(session);
			Address newAddress = this->Storage::write(session, (void*)table);
			if(newAddress == Storage::InvalidAddress) {
				this->rollback(session);
				return ubiq::GenericError::writeError();
			}

			root = newAddress;
		}

		this->commit(session);
		return true;
	} else {
		Iterator iterator(key);
		ubiq::GenericError ret = iterate<FullComparator>(session, iterator);
		if(ret.failed()) {
			this->closeReadWriteSession(session);
			return ubiq::GenericError::outOfMemoryError();
		}

		bool rootHasTwo = ret;

		while (1) {
			void *ret = this->read(session, iterator.currentAddress);

			if(!ret) {
				this->closeReadWriteSession(session);
				return ubiq::GenericError::readError();
			}

			table = (Table*) ret;

			length = table->length();

			position = algorithm::Bisect::find(table->elements, length, key);

			if (position.present()) {
				BtreeTrace::assert(position.single());

				if(returnedValue)
					*returnedValue = table->elements[position.first()].value;

				if (length == Table::splitPoint32_t) {
					PlanOfAction<Table> plan;
					if(actionPlanner(session, plan, iterator.locator).failed()) {
						this->release(session, table);
						this->closeReadWriteSession(session);
						return ubiq::GenericError::readError();
					}

					uint32_t amount;
					this->upgrade(session);
					switch(plan.action){
					case PlanOfAction<Table>::mergeUp:
						table->remove(position.first(), length);

						for(uint32_t i=0; i<Table::splitPoint32_t; i++)
							table->elements[Table::splitPoint32_t - 1 + i] = plan.partner->elements[i];

						table->terminate(2 * Table::splitPoint32_t - 1);

						this->disposeBuffered(session, (void*)plan.partner);
						{
							if(rootHasTwo && !iterator.locator.hasMore())
								this->flagNextAsRoot(session);

							Address newAddress = this->Storage::write(session, table);
							if(newAddress == Storage::InvalidAddress) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							FailAddress newRoot = mergeEntry(session, iterator.locator, newAddress, MergeDirection::Up, rootHasTwo);
							if(newRoot.failed()) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							root = newRoot;
						}
					break;
					case PlanOfAction<Table>::mergeDown:
						for(uint32_t i=0; i<position.first(); i++)
							plan.partner->elements[Table::splitPoint32_t + i] = table->elements[i];

						for(uint32_t i=position.first()+1; i<Table::splitPoint32_t; i++)
							plan.partner->elements[Table::splitPoint32_t + i - 1] = table->elements[i];

						this->disposeBuffered(session, (void*)table);

						plan.partner->terminate(2 * Table::splitPoint32_t - 1);

						{
							if(rootHasTwo && !iterator.locator.hasMore())
								this->flagNextAsRoot(session);

							Address newAddress = this->Storage::write(session, plan.partner);
							if(newAddress == Storage::InvalidAddress) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							FailAddress newRoot = mergeEntry(session, iterator.locator, newAddress, MergeDirection::Down, rootHasTwo);
							if(newRoot.failed()) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							root = newRoot;
						}
					break;
					case PlanOfAction<Table>::redistGreater:
						amount = ((plan.length - Table::splitPoint32_t + 1) / 2);

						table->remove(position.first(), length);

						for (uint32_t i = 0; i < amount; i++)
							table->elements[Table::splitPoint32_t - 1 + i] = plan.partner->elements[i];

						for (uint32_t i = 0; i < plan.length-amount; i++)
							plan.partner->elements[i] = plan.partner->elements[i + amount];

						plan.partner->terminate(plan.length - amount);
						table->terminate(Table::splitPoint32_t + amount - 1);
						{
							IndexKey key(plan.partner->elements[0].key);

							Address newPartnerAddress = this->Storage::write(session, plan.partner);
							if(newPartnerAddress == Storage::InvalidAddress) {
								this->release(session, table);
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							Address newTableAddress = this->Storage::write(session, table);
							if(newTableAddress == Storage::InvalidAddress) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							FailAddress newRoot = updateTwo(session, iterator.locator, UpdateDirection::Up, key, newTableAddress, newPartnerAddress);

							if(newRoot.failed()) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							root = newRoot;
						}
					break;
					case PlanOfAction<Table>::redistSmaller:
					amount = ((plan.length - Table::splitPoint32_t + 1) / 2);

						for (int32_t i = Table::splitPoint32_t - 1; i > position.first(); i--)
							table->elements[i + amount - 1] = table->elements[i];

						for (int32_t i = position.first() - 1; i >= 0; i--)
							table->elements[i + amount] = table->elements[i];

						for (uint32_t i = 0; i < amount; i++)
							table->elements[i] = plan.partner->elements[i + plan.length - amount];

						plan.partner->terminate(plan.length - amount);
						table->terminate(Table::splitPoint32_t + amount - 1);
						{
							IndexKey key(table->elements[0].key);

							Address newPartnerAddress = this->Storage::write(session, plan.partner);
							if(newPartnerAddress == Storage::InvalidAddress) {
								this->release(session, table);
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							Address newTableAddress = this->Storage::write(session, table);
							if(newTableAddress == Storage::InvalidAddress) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							FailAddress newRoot = updateTwo(session, iterator.locator, UpdateDirection::Down, key, newTableAddress, newPartnerAddress);

							if(newRoot.failed()) {
								this->rollback(session);
								return ubiq::GenericError::writeError();
							}

							root = newRoot;
						}
					break;
					}
					this->commit(session);
				} else {
					table->remove(position.first(), length);
					table->terminate(length-1);
					this->upgrade(session);

					Address newAddress = this->Storage::write(session, table);
					if(newAddress == Storage::InvalidAddress) {
						this->rollback(session);
						return ubiq::GenericError::writeError();
					}

					FailAddress newRoot = updateOne(session, iterator.locator, newAddress);
					if(newRoot.failed()) {
						this->rollback(session);
						return ubiq::GenericError::writeError();
					}

					root = newRoot;
					this->commit(session);
				}

				return true;
			}

			this->release(session, table);

			if (!iterator.hasNext() || (!position.present() && position.insertionIndex() == 0)) {
				this->closeReadWriteSession(session);
				return false;
			} else {
				if(step<FullComparator>(session, iterator).failed()) {
					this->rollback(session);
					return ubiq::GenericError::outOfMemoryError();
				}
			}
		}
	}

	return false;
}


#endif /* BTREEDEL_H_ */
