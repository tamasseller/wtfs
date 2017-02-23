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

#include "ubiquitous/Error.h"

class MockFs {
public:
	typedef unsigned int NodeId;
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
	pet::GenericError checkNew(Node&, const char*, const char*);
public:
	pet::GenericError fetchRoot(Node&);
	pet::GenericError fetchChildByName(Node&, const char*, const char*);
	pet::GenericError fetchChildById(Node&, NodeId);
	pet::GenericError fetchFirstChild(Node&);
	pet::GenericError fetchNextSibling(Node&);
	pet::GenericError newDirectory(Node&, const char*, const char*);
	pet::GenericError newFile(Node&, const char*, const char*);
	pet::GenericError removeNode(Node&);

	pet::GenericError openStream(Node&, Stream&);
	pet::GenericError flushStream(Stream&);

	class Node {
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

		pet::GenericError read(void* &content, unsigned int size);
		pet::GenericError write(void* &content, unsigned int size);
		pet::GenericError setPosition(Whence whence, int offset);
		pet::GenericError flush();

		unsigned int getPosition();
		unsigned int getSize();
	};
};

#endif /* MOCKFS_H_ */
