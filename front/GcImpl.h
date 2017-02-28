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

#ifndef GCIMPL_H_
#define GCIMPL_H_

template<class Config>
inline pet::GenericError WtfsEcosystem<Config>::WtfsMain::collectGarbage()
{
	typedef typename Manager::Iterator StorageIterator;
	typedef typename FlashDriver::Address Address;
	typedef typename Buffers::Buffer Buffer;

	nodeListLock.lock();

	bool done = false;

	WtfsTrace::info << "gc invoked\n";

	for(StorageIterator candidateIt = StorageIterator(*this); candidateIt.currentBlock() != -1; candidateIt.step(*this)) {

		WtfsTrace::info << "\tconsidering block #" << candidateIt.currentBlock()<< "\n";

		if(candidateIt.currentCount(*this) == FlashDriver::blockSize) {
			WtfsTrace::info << "\tblock #" << candidateIt.currentBlock() << " is full, aborting !\n";
			break;
		}

		if(this->isBlockBeingUsed(candidateIt.currentBlock())) {
			WtfsTrace::info << "\tblock #" << candidateIt.currentBlock() << " is being used, retrying\n";
			continue;
		}

		Address page = candidateIt.currentBlock() * FlashDriver::blockSize;
		Buffer* buff = this->buffers->find(page);
		int32_t level = (int32_t)buff->data.level;
		this->buffers->release(buff, Clean);

		buffers->flush();

		WtfsTrace::info << "\tmoving out useful data from block #" << candidateIt.currentBlock() << ", contains "
				<< candidateIt.currentCount(*this) << " pages of level "
				<< level << " data\n";

		if(level >= 0) {
			auto moveRet = moveAroundMetaPages(page, candidateIt.currentCount(*this));
			if(moveRet.failed() || !moveRet) {
				WtfsTrace::info << "\t";
				WtfsTrace::warn << "gc freeing up of meta block #" << candidateIt.currentBlock() << " FAILED!\n";
				nodeListLock.unlock();
				return moveRet.rethrow();
			}

			done = true;
			break;
		} else {
			auto moveRet = moveAroundBlobPages(page, candidateIt.currentCount(*this));
			if(moveRet.failed() || !moveRet) {
				WtfsTrace::info << "\t";
				WtfsTrace::warn << "gc freeing up of blob block #" << candidateIt.currentBlock() << " FAILED!\n";
				nodeListLock.unlock();
				return moveRet.rethrow();
			}

			done = true;
			break;
		}
	}

	if(done)
		WtfsTrace::info << "gc operation successful\n\n";
	else
		WtfsTrace::warn << "gc operation FAILED\n\n";

	nodeListLock.unlock();
	return done;
}

template<class Config>
inline pet::GenericError WtfsEcosystem<Config>::WtfsMain::
moveAroundBlobPages(typename FlashDriver::Address const page, uint32_t usedPages)
{
	typedef typename BlobStore::Page Page;
	typedef typename FlashDriver::Address Address;

	for(uint32_t i = 0; usedPages && i < FlashDriver::blockSize; i++) {
		Address movePage = page + i;
		typename Buffers::Buffer* buff = this->buffers->find(movePage);
		Node* node = openNodes.findByFields(((Page*)buff)->meta.id, &Node::key, &FullKey::id);

		if(!node) {
			pet::GenericError rootRes = fetchRoot(tempNode);
			if(rootRes.failed()) {
				return rootRes.rethrow();
			}

			pet::GenericError idRes = fetchById(tempNode, ((Page*)buff)->meta.parentId, ((Page*)buff)->meta.id);
			if(idRes.failed()) {
				return idRes.rethrow();
			}

			node = &this->tempNode;
		}

		this->buffers->release(buff, Clean);

		pet::GenericError travRes = node->relocate(movePage);

		if(travRes.failed()) {
			WtfsTrace::fail << "\terror moving blob page " << page + i << "\n";
			return travRes.rethrow();
		} else if(travRes) {
			usedPages--;

		   pet::GenericError updateRes = this->update(node->key, *node);

		   if(updateRes.failed())
				   return updateRes.rethrow();

			WtfsTrace::info << "\tmoved blob page " << page + i << " -> " << movePage << " (id:" << node->getId() << ")\n";
		}
	}

	return !usedPages;
}

template<class Config>
inline pet::GenericError WtfsEcosystem<Config>::WtfsMain::
moveAroundMetaPages(typename FlashDriver::Address const page, uint32_t usedPages)
{
	typedef typename FlashDriver::Address Address;
	typedef typename MetaTree::Traversor Traversor;

	for(uint32_t i = 0; usedPages && i < FlashDriver::blockSize; i++) {
		Address movePage = page + i;
		pet::GenericError travRes = MetaTree::relocate(movePage);

		if(travRes.failed()) {
			WtfsTrace::fail << "\terror moving meta page " << page + i << "\n";
			return travRes.rethrow();
		} else if(travRes) {
			usedPages--;
			WtfsTrace::info << "\tmoved meta page " << page + i << " -> " << movePage << "\n";
		}
	}

	return !usedPages;
}


#endif /* GCIMPL_H_ */
