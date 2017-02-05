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

#ifndef MOCKFS_H_
#define MOCKFS_H_

#include <vector>
#include <string>

#include <cstdint>

#include "ubiquitous/Error.h"

#include "common/NodeBase.h"

class MockFs {
public:
	typedef uintptr_t NodeId;
	class Node;
	class Stream;
private:
	struct Directory;
	struct Entry {
		std::string name;
		struct Directory* parent;
		virtual bool isDir() = 0;

		Entry(const std::string &, Directory*);
	};

	struct File: public Entry {
		std::vector<char> data;

		inline virtual bool isDir() {
			return false;
		}

		inline File(const std::string &name, Directory* parent): Entry(name, parent) {}
	};

	struct Directory: public Entry {
		std::vector<Entry*> children;

		inline virtual bool isDir() {
			return true;
		}

		inline Directory(const std::string &name, Directory* parent): Entry(name, parent) {}
		inline Directory(): Entry("", 0) {}
	};

	Directory root;
	ubiq::GenericError checkNew(Node&, const char*, const char*);
public:
	ubiq::GenericError fetchRoot(Node&);
	ubiq::GenericError fetchChildByName(Node&, const char*, const char* end = 0);
	ubiq::GenericError fetchChildById(Node&, NodeId);
	ubiq::GenericError fetchFirstChild(Node&);
	ubiq::GenericError fetchNextSibling(Node&);
	ubiq::GenericError newDirectory(Node&, const char*, const char*);
	ubiq::GenericError newFile(Node&, const char*, const char*);
	ubiq::GenericError removeNode(Node&);

	ubiq::GenericError openStream(Node&, Stream&);
	ubiq::GenericError flushStream(Stream&);

	class Node: public NodeBase<Node> {
		Entry* entry;
	public:
		friend MockFs;
		inline Node(): entry(0) {}
		void getName(const char*&, const char*&);
		bool isDirectory();
		NodeId getId();
	};

	class Stream {
		friend MockFs;
		File *file;
		unsigned int offset;
	public:

		inline Stream(): file(0), offset(0) {}

		enum Whence {
			Start, Current, End
		};

		ubiq::GenericError read(void* &content, unsigned int size);
		ubiq::GenericError write(void* &content, unsigned int size);
		ubiq::GenericError setPosition(Whence whence, int offset);
		ubiq::GenericError flush();

		unsigned int getPosition();
		unsigned int getSize();
	};
};

#endif /* MOCKFS_H_ */
