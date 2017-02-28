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

#ifndef PATH_H_
#define PATH_H_

#include "ubiquitous/Error.h"
#include "pool/Stack.h"

#include <cstring>

template<class Fs, class Allocator>
class Path {
	typedef pet::DynamicStack<typename Fs::NodeId, Allocator, 4> IdStack;
	IdStack ids;

public:
	typedef typename IdStack::BottomUpIterator Reader;

	inline Reader getReader() const;
	inline pet::GenericError stepReader(Reader &reader, Fs&, typename Fs::Node&);
	inline pet::GenericError load(Fs&, typename Fs::Node&);
	pet::GenericError append(Fs&, typename Fs::Node&, const char* start, const char* end = 0);

	inline void clear();
	inline void dropLast();

	pet::GenericError copy(const Path& other);
};

template<class Fs, class Allocator>
inline typename Path<Fs, Allocator>::Reader
Path<Fs, Allocator>::getReader() const {
	return ids.bottomUpiterator();
}

template<class Fs, class Allocator>
inline pet::GenericError
Path<Fs, Allocator>::stepReader(Reader& reader, Fs &fs, typename Fs::Node& node)
{
	if(reader.current()) {
		pet::GenericError ret = fs.fetchChildById(node, *(reader.current()));

		if(ret.failed())
			return ret.rethrow();

		reader.step(ids);
		return true;
	}

	return false;
}

template<class Fs, class Allocator>
inline pet::GenericError
Path<Fs, Allocator>::load(Fs &fs, typename Fs::Node& node) {
	Reader reader(getReader());

	pet::GenericError res = fs.fetchRoot(node);

	if(res.failed())
		return res.rethrow();

	while(1) {
		pet::GenericError res = stepReader(reader, fs, node);

		if(res.failed())
			return res.rethrow();

		if(!res)
			break;
	}

	return true;
}

template<class Fs, class Allocator>
pet::GenericError Path<Fs, Allocator>::append(Fs &fs, typename Fs::Node &node, const char* start, const char* strEnd)
{
	pet::GenericError loadRes = load(fs, node);

	if(loadRes.failed())
		return loadRes.rethrow();

	const char *end = start;

	if(*start != '/')
		start--;

	while(1) {
		if((end-start > 0) && (*end == '/' || *end == '\0' || end == strEnd)) {
			if(end - start != 1) { 												// 'foo//bar' case omitted
				if(((end - start) == 2) && (start[1] == '.')) { 				// 'foo/./bar' case check
					if(!node.isDirectory())
						return pet::GenericError::isNotDirectoryError();
				} else {
					if(((end-start) == 3) && (start[1] == '.' && start[2] == '.')) {	// 'foo/../bar' case
						if(!node.isDirectory())
							return pet::GenericError::isNotDirectoryError();

						ids.release();

						pet::GenericError loadRes = load(fs, node);

						if(loadRes.failed())
							return loadRes.rethrow();
					} else {
						pet::GenericError fetchChildRes = fs.fetchChildByName(node, start+1, end);

						if(fetchChildRes.failed())
							return fetchChildRes.rethrow();

						if(ids.acquire().failed())
							return pet::GenericError::outOfMemoryError();

						*ids.current() = node.getId();
					}
				}
			}

			start = end;
		}

		if(*end == '\0' || end == strEnd)
			break;

		end++;
	}

	return true;
}

template<class Fs, class Allocator>
inline void Path<Fs, Allocator>::clear() {
	ids.clear();
}

template<class Fs, class Allocator>
inline void Path<Fs, Allocator>::dropLast() {
	ids.release();
}

template<class Fs, class Allocator>
pet::GenericError Path<Fs, Allocator>::copy(const Path& other) {
	if(ids.copyFrom(other.ids).failed())
		return pet::GenericError::outOfMemoryError();
	return true;
}

#endif /* PATH_H_ */
