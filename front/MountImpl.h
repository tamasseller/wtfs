/*******************************************************************************
 * Copyright (c) 2017 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *     IBM Corporation - initial API and implementation
 *******************************************************************************/
/*
 * MountImpl.h
 *
 *  Created on: 2016.12.10.
 *      Author: tooma
 */

#ifndef MOUNTIMPL_H_
#define MOUNTIMPL_H_

template<class Config>
inline void WtfsEcosystem<Config>::WtfsMain::bind(Buffers* buffers) {
	Buffers::Initializer::initialize(buffers, this);
	this->buffers = buffers;
}

template<class Config>
inline ubiq::GenericError WtfsEcosystem<Config>::WtfsMain::initialize(bool purge)
{
	typedef typename FlashDriver::Address Address;
	typedef typename Buffers::Buffer Buffer;
	typedef typename MetaStore::Page Page;

	if(purge) {
		this->Manager::initWithDefaultAssignment();
		return 0;
	} else {
		int32_t rootLevel = 0;
		uint32_t maxSequenceCounter = 0;
		Address root = FlashDriver::InvalidAddress;

		for(uint32_t i=0; i<FlashDriver::deviceSize; i++) {
			Address page = i * FlashDriver::blockSize;
			Buffer* buff = this->buffers->find(page);

			int32_t level = (int32_t)buff->data.level;
			uint32_t index = this->levelToIndex(level);

			if(this->indexOk(index) && (this->levelAllocations[index].currentAddress == -1u)) {
				if(level >= 0) {
						Address endPage = page + FlashDriver::blockSize;
						while(1) {
							uint32_t sequenceCount = ((Page*)buff)->meta.sequenceNumber;

							if(sequenceCount == -1u) {
								this->levelAllocations[this->levelToIndex(level)].currentAddress = i;
								this->levelAllocations[this->levelToIndex(level)].usedCount = page - i * FlashDriver::blockSize;
								break;
							}

							if(sequenceCount > maxSequenceCounter) {
								root = page;
								maxSequenceCounter = sequenceCount;
								rootLevel = level;
							}

							if(++page == endPage)
								break;

							this->buffers->release(buff, Clean);
							buff = this->buffers->find(page);
						}
				} else if(((Page*)buff)->meta.id != -1u) {
					this->buffers->release(buff, Clean);
					buff = this->buffers->find(page + FlashDriver::blockSize - 1);

					if(((Page*)buff)->meta.id == -1u) {
						this->buffers->release(buff, Clean);
						this->levelAllocations[this->levelToIndex(level)].currentAddress = i;
						uint32_t bottom=1, top=FlashDriver::blockSize-2;

						do{
							uint32_t offset = (bottom + top + 1) / 2;

							buff = this->buffers->find(page + offset);
							if(((Page*)buff)->meta.id == -1u)
								top = offset-1;
							else
								bottom = offset;

							if(bottom == top)
								break;

							this->buffers->release(buff, Clean);
						}while(1);

						this->levelAllocations[this->levelToIndex(level)].usedCount = bottom + 1;
					}
				}
			}

			this->buffers->release(buff, Clean);
		}

		for(uint32_t i=0; i<FlashDriver::deviceSize; i++)
			this->usageCounters[i] = 0;

		this->updateCounter = maxSequenceCounter+1;
		this->root = root;
		this->levels = rootLevel;
		this->maxId = 0;

		typedef typename MetaTree::Traversor Traversor;
		typename MetaStore::ReadWriteSession session(this);

		struct Lambda {
			WtfsMain* self;
			Lambda(WtfsMain* self): self(self) {}

			struct InnerLambda {
				WtfsMain* self;
				InnerLambda(WtfsMain* self): self(self) {}

				typename Config::FlashDriver::Address operator()(
						typename Config::FlashDriver::Address innerAddr,
						uint32_t level, const typename Node::Traversor &parents) {
					self->usageCounters[innerAddr / FlashDriver::blockSize]++;
					return innerAddr;
				}
			};

			typename Config::FlashDriver::Address operator()(
							typename Config::FlashDriver::Address addr,
							uint32_t level, const Traversor &parents)
				{

					self->usageCounters[addr / FlashDriver::blockSize]++;

					if(level == 0) {
						Buffer* buff = self->buffers->find(addr);

						for(uint32_t i = 0; i < ((MetaTable*)buff->data.user)->length(); i++) {
							MetaElement &e = ((MetaTable*)buff->data.user)->elements[i];
							if(e.key.id > self->maxId)
								self->maxId = e.key.id;
							FileTree &temp = self->tempNode;
							temp = e.value;
							self->tempNode.fs = self;

							if(!self->tempNode.isDirectory()) {
								typename BlobStore::ReadWriteSession session(&self->tempNode);
								auto travRes = self->tempNode.traverse(session, InnerLambda(self));
								travRes.failed(); // TODO report
								self->tempNode.closeReadWriteSession(session);
							}
						}

						self->buffers->release(buff, Clean);
					}
					return addr;
				}
		};

		ubiq::GenericError travRet = this->traverse(session, Lambda(this));

		this->closeReadWriteSession(session);

		for(uint32_t i=0; i<Manager::maxLevels; i++) {
			if(this->levelAllocations[i].currentAddress == -1u) {
				this->levelAllocations[i].currentAddress = this->findFree();
				this->levelAllocations[i].usedCount = 0;
			} else {
				this->usageCounters[this->levelAllocations[i].currentAddress] +=
						FlashDriver::blockSize - this->levelAllocations[i].usedCount;
			}
		}

		this->spareCount = 0;
		for(uint32_t i=0; i<FlashDriver::deviceSize; i++)
			if(this->usageCounters[i] == 0)
				this->spareCount++;

		this->maxId++;

		if(travRet.failed())
			travRet.rethrow();

		return 0;
	}
}

#endif /* MOUNTIMPL_H_ */
