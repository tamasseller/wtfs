/*
 * NodeBase.h
 *
 *  Created on: 2017.02.05.
 *      Author: tooma
 */

#ifndef NODEBASE_H_
#define NODEBASE_H_

#include <stdint.h>
#include <string.h>

template<class Child>
class NodeBase {
public:
	void copyName(char* dest, uint32_t buffLength) {
		const char* start, *end;
		((Child*)this)->getName(start, end);

		uint32_t length = end - start;

		if(length > buffLength - 1)
			length = buffLength - 1;

		memcpy(dest, start, length);
		dest[length] = '\0';
	}
};



#endif /* NODEBASE_H_ */
