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

#ifndef OBJECTSTREAM_H_
#define OBJECTSTREAM_H_

#include <cstdint>

#include "ubiquitous/Error.h"

#include <cstring> // memcpy

template<class Stream>
struct ObjectStream: public Stream {
	pet::GenericError writeCopy(const void* src, uint32_t size)
	{
		for(uint32_t n = size; n ;) {
			void* dst;
			pet::GenericError ret = Stream::write(dst, n);

			if(ret.failed())
				return ret.rethrow();

			memcpy(dst, src, ret);
			src = (uint8_t*)src + ret;
			n -= ret;
		}

		return size;
	}

	pet::GenericError readCopy(void* dst, uint32_t size) {
		for(uint32_t n = size; n ;) {
			void* src;
			pet::GenericError ret = Stream::read(src, n);

			if(ret.failed()) {
				if(ret == pet::GenericError::invalidSeek)
					return 0;

				return ret.rethrow();
			}

			if(!ret)
				return size-n;

			memcpy(dst, src, ret);
			dst = (uint8_t*)dst + ret;
			n -= ret;
		}

		return size;
	}
};


#endif /* OBJECTSTREAM_H_ */
