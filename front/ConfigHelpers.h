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
 * ConfigHelpers.h
 *
 *  Created on: 2016.12.17.
 *      Author: tooma
 */

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
