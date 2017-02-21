/*******************************************************************************
 *
 * Copyright (c) 2017 Tamás Seller. All rights reserved.
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
#ifndef CONFIGHELPERS_H_
#define CONFIGHELPERS_H_

struct DefaultNolockConfig {
	struct Mutex {
		inline void lock() {}
		inline void unlock() {}
	};
	struct RWLock {
		inline void readerEnter() {}
		inline void writerEnter() {}
		inline void readerLeave() {}
		inline void writerLeaveUnupgraded() {}
		inline void writerUpgrade() {}
		inline void writerLeaveUpgraded() {}
	};
};

template<class BackendMutex>
struct DefaultMutexConfig
{
	typedef BackendMutex Mutex;

	class RWLock {
		Mutex mutex;
	public:
		inline void readerEnter() {mutex.lock();}
		inline void writerEnter() {mutex.lock();}
		inline void readerLeave() {mutex.unlock();}
		inline void writerLeaveUnupgraded() {mutex.unlock();}
		inline void writerUpgrade() {}
		inline void writerLeaveUpgraded() {mutex.unlock();}
	};
};

template<class BackendMutex, class BackendRWLock>
class DefaultRwlockConfig {
public:
	typedef BackendMutex Mutex;

	class RWLock {
		BackendRWLock lock;
	public:
		inline void readerEnter() {lock.rlock();}
		inline void writerEnter() {lock.wlock();}
		inline void readerLeave() {lock.runlock();}
		inline void writerLeaveUnupgraded() {lock.wunlock();}
		inline void writerUpgrade() {}
		inline void writerLeaveUpgraded() {lock.wunlock();}
	};
};

#endif /* CONFIGHELPERS_H_ */
