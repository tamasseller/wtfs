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

#ifndef STREAMIMPL_H_
#define STREAMIMPL_H_

#include <cstdint>

#include "Wtfs.h"

#include <errno.h>

template<class Config>
pet::GenericError
inline WtfsEcosystem<Config>::WtfsMain::openStream(Node& node, Stream& stream)
{
	if(!node.hasData())
		return pet::GenericError::isDirectoryError();

	nodeListLock.lock();
	Node* openedNode = openNodes.findByFields(node.key.id, &Node::key, &FullKey::id);

	if(openedNode && (openedNode != &node)) {
		nodeListLock.unlock();
		return pet::GenericError::alreadyInUseError();
	}

	node.referenceCount++;

	stream.initialize(&node);

	if(node.referenceCount == 1) {
		openNodes.add(&node);
		nodeListLock.unlock();
		return this->get(node.key, node);
	}

	nodeListLock.unlock();
	return 0;
}

template<class Config>
pet::GenericError
inline WtfsEcosystem<Config>::WtfsMain::flushStream(Stream& stream) {
	pet::GenericError ret = stream.flush();

	if(ret.failed())
		return ret.rethrow();

	if(!stream.node->dirty)
		return 0;

	if(isReadonly)
		return pet::GenericError::readOnlyFsError();

	stream.node->dirty = false;

	return this->update(stream.node->key, *stream.node);
}

template<class Config>
pet::GenericError
inline WtfsEcosystem<Config>::WtfsMain::closeStream(Stream& stream)
{
	pet::GenericError ret = flushStream(stream);

	if(ret.failed())
		return ret.rethrow();

	nodeListLock.lock();
	stream.node->referenceCount--;

	if(!stream.node->referenceCount)
		openNodes.remove(stream.node);

	nodeListLock.unlock();

	stream.node = 0;

	return 0;
}


template<class Config>
inline WtfsEcosystem<Config>::WtfsMain::Stream::Stream():
	node(0), page(0), offset(0), buffer(0), written(false) {}

template<class Config>
inline void WtfsEcosystem<Config>::WtfsMain::Stream::initialize(Node* node) {
	this->node = node;
	written = false;
	buffer = 0;
	offset = 0;
	page = 0;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::Stream::fetchPage()
{
	if(getPosition() > node->getSize())
		return pet::GenericError::invalidSeekError();

	pet::FailPointer<void> ret = node->read(page);

	if(ret.failed())
		return pet::GenericError::readError();

	buffer = ret;
	return 0;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::Stream::flush()
{
	if(written) {
		if(node->fs->isReadonly)
			return pet::GenericError::readOnlyFsError();

		node->dirty = true;
		pet::GenericError ret = node->update(page, getPosition(), buffer);
		buffer = 0;
		written = false;
		return ret;
	} else if(buffer) {
		node->release(buffer);
		buffer = 0;
	}

	return 0;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::Stream::access(void* &content, uint32_t size, bool reading)
{
	if(offset == BlobStore::pageSize) {
		pet::GenericError ret = flush();

		if(ret.failed())
			return ret.rethrow();

		page++;
		offset = 0;
	}

	uint32_t spaceLeft = BlobStore::pageSize - offset;

	if(reading) {
		const uint32_t lastByteIdx = node->getSize() - 1;
		const uint32_t lastPageIdx = lastByteIdx / BlobStore::pageSize;
		if (lastPageIdx == page) {
			const uint32_t lastDataByteIdxOnLastPage = lastByteIdx % BlobStore::pageSize;
			spaceLeft = lastDataByteIdxOnLastPage + 1 - offset;
		}
	}

	if(size > spaceLeft)
		size = spaceLeft;

	if(!buffer) {
		if(getPosition() == node->getSize() && offset == 0) {
			if(node->fs->isReadonly)
				return pet::GenericError::readOnlyFsError();

			pet::FailPointer<void> ret = node->empty();

			if(ret.failed())
				return pet::GenericError::writeError();

			buffer = ret;
		} else {
			pet::GenericError ret = fetchPage();

			if(ret.failed())
				return ret.rethrow();
		}
	}

	content = (uint8_t*)buffer + offset;
	offset += size;
	return size;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::Stream::read(void* &content, uint32_t size)
{
	if(size + getPosition() > node->getSize())
		size = node->getSize()-getPosition();

	if(!size)
		return 0;

	return access(content, size, true);
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::Stream::write(void* &content, uint32_t size)
{
	if(!size)
		return 0;

	pet::GenericError ret = access(content, size, false);

	if(ret.failed())
		return ret.rethrow();

	written = true;

	return ret;
}

template<class Config>
pet::GenericError
WtfsEcosystem<Config>::WtfsMain::Stream::setPosition(Whence whence, int32_t offset)
{
	uint32_t newPosition;

	uint32_t size = ((node->getSize() > getPosition()) ? node->getSize() : getPosition());

	switch(whence) {
	case Start:
		if((offset < 0) || ((uint32_t)offset > size))
			return pet::GenericError::invalidSeekError();

		newPosition = offset;
		break;
	case Current:
		if((offset < -(int32_t)getPosition()))
			return pet::GenericError::invalidSeekError();

		newPosition = getPosition() + offset;
		break;
	case End:
		if(((uint32_t)-offset > size) || (offset > 0))
			return pet::GenericError::invalidSeekError();

		newPosition = size + offset;
		break;
	}

	if(newPosition > size)
		return pet::GenericError::invalidSeekError();

	const uint32_t oldPage = page;
	const uint32_t newPage = newPosition / BlobStore::pageSize;

	if(buffer) {
		if(written) {
			pet::GenericError ret = flush();

			if(ret.failed())
				return ret.rethrow();

		} else if(oldPage != newPage) {
			node->release(buffer);
			buffer = 0;
		}
	}

	page = newPage;
	this->offset = newPosition % BlobStore::pageSize;
	return 0;
}

template<class Config>
inline uint32_t WtfsEcosystem<Config>::WtfsMain::Stream::getPosition() {
	return page * BlobStore::pageSize + offset;
}

template<class Config>
inline uint32_t WtfsEcosystem<Config>::WtfsMain::Stream::getSize() {
	return node->getSize();
}

#endif /* STREAMIMPL_H_ */
