/*******************************************************************************
 *
 * Copyright (c) 2016 Seller Tamás. All rights reserved.
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

#ifndef MOCKSTORAGE_H_
#define MOCKSTORAGE_H_

#include "CppUTest/TestHarness.h"
#include "FailureSource.h"
#include "ubiquitous/Trace.h"

// CppUTest magic trickery avoidance.
#undef new

#include <cstring>
#include <unordered_set>
#include <map>

class MockStorageTrace;

struct StorageFailureSource: public StaticFailureSource<StorageFailureSource> {
	static bool shouldFail() {
		return instance.shouldSimulateError();
	}

	virtual const char* getFailureSourceName() {
		return "Storage";
	}

	inline virtual ~StorageFailureSource() {}
};

/*
 * The storage abstraction operates with the following concepts:
 * 	   -> Storage::Address is a type that represents an address that can be used to retrive
 * 	   	  stored data.
 *
 *     -> Storage::InvalidAddress is a constant that represents some kind of failure of the
 *     	  absence of something. It is an error to attempt a read from this address.
 *
 *     -> Storage::pageSize is the size (in bytes) of a logical page that is used by the storage
 *        read and empty methods return a buffer of this size. The user is expected to handle
 *        the returned void* values as pointers to a block of this size.
 *
 * 	   -> Storage::ReadOnlySession is a read session, that is, through it the user can read stored
 * 	      data, this is done via buffers, which are untyped (ie. void*) a buffer can be
 * 	      allocated and filled with stored data by issuing a call to the read method, when
 * 	      done buffers are needed to be released by the user (release method).
 *
 * 	   -> Storage::ReadWriteSession can be obtained by upgrading a read session, the constructor
 * 	      is required to take care of additional locking, needed for the upgrade. It provides
 * 	      methods of storing modified (or newly created) data. A read-write session can be used
 * 	      allocate unused, empty storage area, with its empty method, also it can be used dispose
 * 	      of storage not needed anymore (dispose methods). During the read-write session
 * 	      modifications are recorded to provide failure handling capability. If a call to the
 * 	      commit method is not issued before the ReadWriteSession is destroyed the changes are
 * 	      expected to be rolled back. If the whole operation is considered successful, the user
 * 	      is required to call the commit method in order to make the modifications persistent.
 * 	      There are two variants of dispose one for buffered data, and another one for not data
 * 	      not read in. The rationale is that the underlying implementation has to know the
 * 	      addresses of open buffers, so the user can be freed from the requirement of keeping
 * 	      track of the addresses.
 * 	      The empty method expects a level parameter, it can be viewed as a hint from the user
 * 	      side, it makes possible the separation of the levels of tree like structures. There are
 * 	      no checks about the value of this parameter (at least in this mock class).
 */

/**
 * This mock class is used to inject failures to the page read, write and empty allocation methods.
 * It includes checks for unreleased buffers and provides several other diagnostic features. Also
 * the correct use of committing is checked, if there were no simulated errors the transaction is
 * expected to be committed.
 */
template<unsigned int pageSizeParam, class Client, bool strict=true, bool checkRoot = true>
struct MockStorage: protected pet::Trace<MockStorageTrace> {
	////////////////////////////////////////////////////////////////
	// These are the required definitions mentioned above
	////////////////////////////////////////////////////////////////

	typedef unsigned int Address;
	static const Address InvalidAddress;
	static const unsigned int pageSize = pageSizeParam;

	////////////////////////////////////////////////////////////////
	// These are basically internals, left public for debug hackery
	////////////////////////////////////////////////////////////////

	/**
	 * PageData represents the payload of a stored page, this is the thing that has to
	 * be sized according the pageSize constant.
	 */
	struct PageData {
		unsigned int data[(pageSize+sizeof(unsigned int)-1)/sizeof(unsigned int)];
		PageData() {
			bzero(data, sizeof(data));
		}
	};

	/**
	 * PageBuffer represents a buffer with all additional meta info (not accessible to the user),
	 * for the purposes of the mock it has a pointer to a possibly existent previous stored version,
	 * it is null for new pages, and a pointer for the pre-existing data at the given location for
	 * reads. It is used for checking the necessity of writes.
	 */
	struct PageBuffer: PageData {
		const PageBuffer* old;
		int level;

		inline PageBuffer(int level): old(0), level(level) {}
		inline PageBuffer(const PageBuffer &other): old(&other), level(other.level) {
			*(PageData*)this = *((const PageData*)&other);
		}
	};

	/**
	 * Diagnostics class includes counters, to examine the i/o activity, and also containers to keep
	 * track of opened buffers and stored data. The trace methods can also output a trace of actions
	 * for debugging purposes.
	 */
	struct Diagnostics {
		unsigned int opened = 0;
		unsigned int closed = 0;
		unsigned int inuse = 0;
		unsigned int maxInUse = 0;
		unsigned int nRead = 0;
		unsigned int nWrite = 0;

		std::unordered_set<void*> open;
		std::unordered_set<Address> stored;

		inline void traceEmpty(void*);
		inline void tracePreWrite(void*);
		inline void tracePostWrite(Address);
		inline void tracePreRead(Address);
		inline void tracePostRead(void*);
		inline void traceRelease(void*);
		inline void traceDispose(const void*);
	};

	static Diagnostics diagnostics;

	////////////////////////////////////////////////////////////////////////
	// This is the business part, an actual implementation should be similar
	////////////////////////////////////////////////////////////////////////

	class ReadOnlySession {
		friend MockStorage;
		bool error, closed;
		std::unordered_set<PageBuffer*> active;
		std::map<int, int> levelEmptyCounters;
		std::map<int, int> levelWriteCounters;
	public:
		inline ReadOnlySession(MockStorage* self): error(false), closed(false) {}
		inline ~ReadOnlySession();
	};

	inline void closeReadOnlySession(ReadOnlySession& session);

	inline void *read(ReadOnlySession& session, Address p);
	inline void release(ReadOnlySession& session, void* p);

	struct ReadWriteSession: public ReadOnlySession {
		friend MockStorage;
		std::unordered_set<PageBuffer*> garbage;
		std::unordered_set<PageBuffer*> newish;
		bool upgraded = false;
		bool nextIsRoot = false;
		bool rootWritten = false;
		bool clean = true;
	public:
		inline ReadWriteSession(MockStorage* self): ReadOnlySession(self) {}
		inline ~ReadWriteSession();
	};

	inline void upgrade(ReadWriteSession& session);
	inline void *empty(ReadWriteSession& session, int unused);
	inline void flagNextAsRoot(ReadWriteSession& session);
	inline Address write(ReadWriteSession& session, void* p);
	inline void disposeBuffered(ReadWriteSession& session, void* p);
	inline void disposeAddress(ReadWriteSession& session, Address p);
	inline void rollback(ReadWriteSession& session);
	inline void commit(ReadWriteSession& session);
	inline void closeReadWriteSession(ReadWriteSession& session);

	bool allowUnnecessary = false;
	inline static bool isClean();
};

///////////////////////////////////////////////////////////////////////////////////////////////

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
const typename MockStorage<pageSizeParam, Client, strict, checkRoot>::Address MockStorage<pageSizeParam, Client, strict, checkRoot>::InvalidAddress = 0;

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
typename MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics MockStorage<pageSizeParam, Client, strict, checkRoot>::diagnostics;


template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::closeReadOnlySession(ReadOnlySession& session) {
	session.closed = true;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::closeReadWriteSession(ReadWriteSession& session) {
	session.closed = true;
}

/**
 * An actual destructor of a ReadOnlySession has to do nothing, here it is checked that
 * there is really nothing to do (ie. no buffers left unreleased)
 */
template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline MockStorage<pageSizeParam, Client, strict, checkRoot>::ReadOnlySession::~ReadOnlySession()
{
	CHECK(this->closed);

	info << "read session termination\n\n";
	if(this->active.size() > (strict ? 0 : 1)) {
		info << "\tBuffers still in use at cleanup: ";

		for(auto x: this->active) {
			info << x << " ";
			delete (PageBuffer*)x;
		}

		info << "\n";
		FAIL_ALWAYS("Leaked buffers")
	}
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void *MockStorage<pageSizeParam, Client, strict, checkRoot>::read(ReadOnlySession& session, Address p)
{
	CHECK(!session.closed);
	if(StorageFailureSource::shouldFail()) {
		info << "\nFAIL read: " << p << "\n\n";
		session.error = true;
		return 0;
	}

	diagnostics.tracePreRead(p);

	void *ret = (PageData*)(new PageBuffer(*(PageBuffer*)(PageData*)(void*)p));
	CHECK(session.active.insert((PageBuffer*)ret).second);

	diagnostics.tracePostRead(ret);
	return (void*) ret;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::release(ReadOnlySession& session, void* p)
{
	CHECK(!session.closed);
	const PageData* old = ((PageBuffer*)(PageData*)(void*)p)->old;
	PageData* current = ((PageData*)(void*)p);

	if(strict) {
		CHECK(old);
		CHECK_TEXT(memcmp(old, current, sizeof(PageData)) == 0, "Released modified data without write");
	}

	auto ret = session.active.erase((PageBuffer*)current);
	if(strict) {
		CHECK(ret == 1);
	}

	delete (PageBuffer*)current;

	diagnostics.traceRelease(p);
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::upgrade(ReadWriteSession& session)
{
	CHECK(!session.closed);
	CHECK(!session.upgraded);
	session.upgraded = true;
}


template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::flagNextAsRoot(ReadWriteSession& session)
{
	session.nextIsRoot = true;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
typename MockStorage<pageSizeParam, Client, strict, checkRoot>::Address
MockStorage<pageSizeParam, Client, strict, checkRoot>::write(ReadWriteSession& session, void* p)
{
	CHECK(!session.rootWritten);

	CHECK(!session.closed);
	CHECK(session.upgraded);
	diagnostics.tracePreWrite(p);

	const PageData* old = ((PageBuffer*)(PageData*)(void*)p)->old;
	PageData* current = ((PageData*)(void*)p);

	if(old) {
		if(strict) {
			bool ok = allowUnnecessary || memcmp(old, current, sizeof(PageData)) != 0;
			CHECK_TEXT(ok, "Unnecessary write");
		}

		CHECK(session.garbage.insert((PageBuffer*)old).second);
		((PageBuffer*)current)->old = 0;
	}

	auto ret = session.active.erase((PageBuffer*)current);
	if(strict) {
		CHECK(ret == 1);
	}

	if(StorageFailureSource::shouldFail()) {
		info << "\nFAIL write: " << p << "\n\n";
		session.error = true;
		return 0;
	}

	CHECK(session.newish.insert((PageBuffer*)current).second);

	auto it = session.levelWriteCounters.find(((PageBuffer*)current)->level);
	if (it != session.levelWriteCounters.end())
		(*it).second++;
	else
		session.levelWriteCounters.insert(std::pair<int, int>(((PageBuffer*)current)->level, 1));


	diagnostics.tracePostWrite((Address)p);

	if(old)
		diagnostics.traceDispose(old);

	if(session.nextIsRoot) {
		session.rootWritten = true;
	}

	session.clean = false;

	return (Address) p;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void *MockStorage<pageSizeParam, Client, strict, checkRoot>::empty(ReadWriteSession& session, int level)
{
	CHECK(!session.closed);

	auto it = session.levelEmptyCounters.find(level);
	if (it != session.levelEmptyCounters.end())
		(*it).second++;
	else
		session.levelEmptyCounters.insert(std::pair<int, int>(level, 1));

	CHECK(session.upgraded);
	if(StorageFailureSource::shouldFail()) {
		info << "\nFAIL empty\n\n";
		session.error = true;
		return 0;
	}

	PageData *ret = new PageBuffer(level);
	CHECK(session.active.insert((PageBuffer*)ret).second);

	diagnostics.traceEmpty(ret);
	return (void*)ret;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::disposeBuffered(ReadWriteSession& session, void* p)
{
	CHECK(!session.closed);

	CHECK(session.upgraded);
	const PageData* old = ((PageBuffer*)(PageData*)(void*)p)->old;
	PageData* current = ((PageData*)(void*)p);

	CHECK_TEXT(old != NULL, "Newly allocated page disposed");
	CHECK_TEXT(memcmp(old, current, sizeof(PageData)) == 0, "Disposed of modified data");

	CHECK(session.active.erase((PageBuffer*)current) == 1);
	CHECK(session.garbage.insert((PageBuffer*)old).second);
	delete (PageBuffer*)current;

	diagnostics.traceDispose(old);
	diagnostics.traceRelease(p);
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
void MockStorage<pageSizeParam, Client, strict, checkRoot>::disposeAddress(ReadWriteSession& session, Address p)
{
	CHECK(!session.closed);
	CHECK(session.upgraded);

	const PageData* old = ((PageBuffer*)(PageData*)(void*)p)->old;
	PageData* current = ((PageData*)(void*)p);

	CHECK(old == NULL);

	CHECK(session.garbage.insert((PageBuffer*)p).second);

	diagnostics.traceDispose((void*)p);
}

/**
 * Here the recorded changes are checked to be valid, items to be trashed exist and
 * new stuff do not.
 */
template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::commit(ReadWriteSession& session)
{
	CHECK(!session.closed);
	CHECK(session.upgraded);
	CHECK(!session.error);
	info << "commit:\n";

	if(!session.garbage.empty()) {
		info << "\ttrashed: ";

		for(auto x: session.garbage) {
			CHECK(diagnostics.stored.find((Address)x) == diagnostics.stored.end());
			info << x << " ";
			delete (PageBuffer*)x;
		}

		info << "\n";
	}

	for(auto x: session.newish) {
		CHECK(diagnostics.stored.find((Address)x) != diagnostics.stored.end());
	}

	session.garbage.clear();
	session.newish.clear();

	session.closed = true;
}

/**
 * The destructor of a ReadWriteSession has to revert to changes that are not commited
 */
template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::rollback(ReadWriteSession& session)
{
	CHECK(!session.closed);
	CHECK(session.upgraded);

	info << "write session rollback\n\n";

	if(!session.garbage.empty() || !session.newish.empty()) {
		CHECK(session.error);
		info << "rollback:\n";
		if(!session.garbage.empty())
			info << "\trestored: ";

		for(auto x: session.garbage) {
			CHECK(diagnostics.stored.insert((Address)x).second);
			info << x << " ";
		}

		if(!session.garbage.empty())
			info << "\n";

		if(!session.newish.empty())
			info << "\tfailed-new deleted: ";

		for(auto x: session.newish) {
			CHECK(diagnostics.stored.erase((Address)x) == 1);
			info << x << " ";
			delete (PageBuffer*)x;
		}

		if(!session.newish.empty())
			info << "\n";
	}

	session.closed = true;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline MockStorage<pageSizeParam, Client, strict, checkRoot>::ReadWriteSession::~ReadWriteSession()
{
	CHECK(this->closed);

	bool ok = garbage.empty() && newish.empty();

	for(auto x: this->levelEmptyCounters)
		CHECK(x.second == 1);

	for(auto x: this->levelWriteCounters)
		CHECK(x.second <= 2);

	if(checkRoot)
		CHECK(clean || rootWritten);

	CHECK(ok);
}


template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline bool MockStorage<pageSizeParam, Client, strict, checkRoot>::isClean() {
	info << "\nisClean called\n\n";

	if(diagnostics.stored.empty() && diagnostics.open.empty())
		return true;

	info << "Leaked buffers: ";

	for(auto x: diagnostics.open)
		info << x << " ";

	info << "\n";

	info << "Still in use: ";

	for(auto x: diagnostics.stored) {
		info << x << " ";
		delete (PageBuffer*)x;
	}

	diagnostics.stored.clear();

	info << "\n";

	return false;
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::traceEmpty(void* ret)
{
	CHECK_TEXT(diagnostics.open.insert((void*)ret).second, "Duplicate empty");

	diagnostics.opened++;
	diagnostics.inuse++;

	if(diagnostics.maxInUse < diagnostics.inuse)
		diagnostics.maxInUse = diagnostics.inuse;

	info << "empty" << ret << "\n";
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::tracePreWrite(void* in)
{
	info << "write " << in;
	CHECK_TEXT(diagnostics.open.erase((void*)in) == 1, "Unopened buffer written");
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::tracePostWrite(Address ret)
{
	CHECK(diagnostics.stored.insert((Address)ret).second);
	diagnostics.closed++;
	diagnostics.nWrite++;
	info << " -> " << ret << "\n";
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::tracePreRead(Address in)
{
	bool ok = diagnostics.stored.find(in) != diagnostics.stored.end();
	CHECK_TEXT(ok, "Invalid read");
	info << "read " << in << " -> ";
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::tracePostRead(void* ret)
{
	CHECK_TEXT(diagnostics.open.insert((void*)ret).second, "Duplicate read");
	diagnostics.nRead++;
	diagnostics.opened++;
	info << ret << "\n";
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::traceRelease(void* in)
{
	CHECK_TEXT(diagnostics.open.erase((void*)in) == 1, "Unopened buffer released");
	diagnostics.closed++;
	info << "release " << in << "\n";
}

template<unsigned int pageSizeParam, class Client, bool strict, bool checkRoot>
inline void MockStorage<pageSizeParam, Client, strict, checkRoot>::Diagnostics::traceDispose(const void* in)
{
	CHECK_TEXT(diagnostics.stored.erase((Address)in) == 1, "Not stored buffer disposed");
	info << "dispose " << in << "\n";
	diagnostics.inuse--;
}

#endif /* MOCKSTORAGE_H_ */
