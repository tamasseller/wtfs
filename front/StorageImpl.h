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

#ifndef STORAGEIMPL_H_
#define STORAGEIMPL_H_

template<class BackendConfig, class Allocator, class Child>
inline void StorageBase<BackendConfig, Allocator, Child>::checkGc()
{
	auto &fs = Child::getFs(this);

	fs.inGc = true;

	while(fs.gcNeeded()) {
		auto ret = fs.collectGarbage();
		if(ret.failed() || !ret) {
			fs.isReadonly = true;
			break;
		}
	}

	fs.inGc = false;
}

template<class BackendConfig, class Allocator, class Child>
inline void StorageBase<BackendConfig, Allocator, Child>::rollback(ReadWriteSession& session)
{
	auto &fs = Child::getFs(this);

	while(1) {
		Address addr;
		if(!session.garbage.readOne(addr))
			break;

		fs.claim(addr);
	}

	while(1) {
		Address addr;
		if(!session.newish.readOne(addr))
			break;

		fs.reclaim(addr);
	}

	if(!fs.inGc)
		checkGc();

	Child::getFs(this).writerLeaveUpgraded();
}

template<class BackendConfig, class Allocator, class Child>
inline void StorageBase<BackendConfig, Allocator, Child>::commit(ReadWriteSession& session)
{
	if(!Child::getFs(this).inGc)
		checkGc();

	Child::getFs(this).writerLeaveUpgraded();
}


template<class BackendConfig, class Allocator, class Child>
inline StorageBase<BackendConfig, Allocator, Child>::ReadOnlySession::ReadOnlySession(StorageBase* self)
{
	Child::getFs(self).readerEnter();
}

template<class BackendConfig, class Allocator, class Child>
inline StorageBase<BackendConfig, Allocator, Child>::ReadWriteSession::ReadWriteSession(StorageBase* self)
{
	Child::getFs(self).writerEnter();
}


template<class BackendConfig, class Allocator, class Child>
inline void StorageBase<BackendConfig, Allocator, Child>::closeReadOnlySession(ReadOnlySession& session)
{
	Child::getFs(this).readerLeave();
}

template<class BackendConfig, class Allocator, class Child>
inline void StorageBase<BackendConfig, Allocator, Child>::closeReadWriteSession(ReadWriteSession& session)
{
	Child::getFs(this).writerLeaveUnupgraded();
}

template<class BackendConfig, class Allocator, class Child>
inline void StorageBase<BackendConfig, Allocator, Child>::upgrade(ReadWriteSession& session)
{
	Child::getFs(this).writerUpgrade();
}

template<class BackendConfig, class Allocator, class Child>
void *StorageBase<BackendConfig, Allocator, Child>::empty(ReadWriteSession& session, int32_t level)
{
	typename Child::Buffers::Buffer* ret = Child::getFs(this).buffers->find(BackendConfig::InvalidAddress);
	ret->data.level = level;
	return ret;
}

template<class BackendConfig, class Allocator, class Child>
void *StorageBase<BackendConfig, Allocator, Child>::read(ReadOnlySession& session, Address p)
{
	return Child::getFs(this).buffers->find(p);
}

template<class BackendConfig, class Allocator, class Child>
typename StorageBase<BackendConfig, Allocator, Child>::Address
StorageBase<BackendConfig, Allocator, Child>::write(ReadWriteSession& session, void* p)
{
	auto &storage = Child::getFs(this).buffers;
	Address oldAddress = storage->getAddress((typename Child::Buffers::Buffer*)p);

	Address ret = storage->release((typename Child::Buffers::Buffer*)p, BufferReleaseCondition::Dirty);

	if(oldAddress != BackendConfig::InvalidAddress)
		if(!session.garbage.writeOne(oldAddress))
			return BackendConfig::InvalidAddress;

	if(ret != BackendConfig::InvalidAddress)
		if(!session.newish.writeOne(ret))
			return BackendConfig::InvalidAddress;

	return ret;
}

template<class BackendConfig, class Allocator, class Child>
void StorageBase<BackendConfig, Allocator, Child>::release(ReadOnlySession& session, void* p)
{
	Child::getFs(this).buffers->release((typename Child::Buffers::Buffer*)p, BufferReleaseCondition::Clean);
}

template<class BackendConfig, class Allocator, class Child>
void StorageBase<BackendConfig, Allocator, Child>::disposeBuffered(ReadWriteSession& session,void* p)
{
	auto &storage = Child::getFs(this).buffers;

	session.garbage.writeOne(storage->getAddress((typename Child::Buffers::Buffer*)p));

	storage->release((typename Child::Buffers::Buffer*)p, BufferReleaseCondition::Purge);
}

template<class BackendConfig, class Allocator, class Child>
void StorageBase<BackendConfig, Allocator, Child>::disposeAddress(ReadWriteSession& session, Address p)
{
	session.garbage.writeOne(p);
	Child::getFs(this).reclaim(p);
}

#endif /* STORAGEIMPL_H_ */
