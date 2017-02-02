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

#include "MockFs.h"

#include <algorithm>

#include "FailureInjectorPlugin.h"

struct FsFailureSource: public StaticFailureSource<FsFailureSource> {
	static bool shouldFail() {
		return instance.shouldSimulateError();
	}

	inline virtual ~FsFailureSource(){};
};

MockFs::Entry::Entry(const std::string &name, struct Directory* parent):
	name(name), parent(parent)
{
	if(parent)
		parent->children.push_back(this);
}

ubiq::GenericError MockFs::fetchRoot(Node& node)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	node.entry = &root;
	return true;
}

ubiq::GenericError MockFs::fetchChildByName(Node& node, const char* start, const char* end)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	if(!node.entry->isDir())
		return ubiq::GenericError::isNotDirectory;

	std::string name(start, end-start);
	for(auto x: ((Directory*)node.entry)->children) {
		if(x->name == name) {
			node.entry = x;
			return true;
		}
	}

	return ubiq::GenericError::noSuchEntryError();
}

ubiq::GenericError MockFs::fetchChildById(Node& node, NodeId id)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	if(!node.entry->isDir())
		return ubiq::GenericError::isNotDirectory;

	for(auto x: ((Directory*)node.entry)->children) {
		if((unsigned int)x == id) {
			node.entry = x;
			return true;
		}
	}

	return ubiq::GenericError::noSuchEntryError();
}

ubiq::GenericError MockFs::fetchFirstChild(Node& node)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	if(!node.entry->isDir())
		return ubiq::GenericError::isNotDirectoryError();

	auto &children = ((Directory*)node.entry)->children;
	auto x = children.begin();

	if(x == children.end())
		return false;

	node.entry = *x;
	return true;
}

ubiq::GenericError MockFs::fetchNextSibling(Node& node)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	if(!node.entry->parent) // the root has no siblings
		return false;

	auto &children = node.entry->parent->children;
	auto x = std::find(children.begin(), children.end(), node.entry);

	if(x == children.end()) // current not found, WTF ??
		return ubiq::GenericError::noSuchEntryError();

	++x;

	if(x == children.end()) // last child
		return false;

	node.entry = *x;
	return true;
}

ubiq::GenericError MockFs::checkNew(Node& node, const char* start, const char* end)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	std::string name(start, end-start);
	for(auto x: ((Directory*)node.entry)->children)
		if(x->name == name)
			return ubiq::GenericError::alreadyExistsError();

	return true;
}

ubiq::GenericError MockFs::newDirectory(Node& node, const char* start, const char* end)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::writeError();

	ubiq::GenericError ret = checkNew(node, start, end);
	if(ret.failed())
		return ret.rethrow();

	node.entry = new Directory(std::string(start, end-start), (Directory*)node.entry);
	return true;
}

ubiq::GenericError MockFs::newFile(Node& node, const char* start, const char* end)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::writeError();

	ubiq::GenericError ret = checkNew(node, start, end);
	if(ret.failed())
		return ret.rethrow();

	node.entry = new File(std::string(start, end-start), (Directory*)node.entry);
	return true;
}

ubiq::GenericError MockFs::removeNode(Node& node)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::writeError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	if(!node.entry->parent) // cannot remove root
		return ubiq::GenericError::invalidArgumentError();

	if(node.entry->isDir() && !((Directory*)node.entry)->children.empty())
		return ubiq::GenericError::notEmptyError();

	auto &children = node.entry->parent->children;
	auto x = std::find(children.begin(), children.end(), node.entry);

	if(x == children.end())
		return ubiq::GenericError::noSuchEntryError();

	children.erase(x);

	return true;
}

ubiq::GenericError MockFs::openStream(Node& node, Stream& stream)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!node.entry)
		return ubiq::GenericError::invalidArgumentError();

	if(node.entry->isDir())
		return ubiq::GenericError::isDirectoryError();

	stream.file = (File*)node.entry;
	return true;
}

ubiq::GenericError MockFs::flushStream(Stream&) {
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	return true;
}

bool MockFs::Node::isDirectory()
{
	if(!entry)
		return false;

	return entry->isDir();
}

void MockFs::Node::getName(const char*&begin, const char*&end)
{
	if(!entry) {
		begin = end = 0;
		return;
	}

	begin = entry->name.c_str();
	end = entry->name.c_str() + entry->name.length();
}

MockFs::NodeId MockFs::Node::getId() {
	return (NodeId)entry;
}

ubiq::GenericError MockFs::Stream::read(void* &content, unsigned int size)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::readError();

	if(!file)
		return ubiq::GenericError::invalidArgumentError();

	if(offset >= file->data.size())
		return 0;

	content = file->data.data() + offset;

	if(offset + size > file->data.size())
		return file->data.size() - size;

	offset += size;
	return size;
}

ubiq::GenericError MockFs::Stream::write(void* &content, unsigned int size)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::writeError();

	if(!file)
		return ubiq::GenericError::invalidArgumentError();

	while(file->data.size() < offset + size)
		file->data.push_back('\0');

	content = file->data.data() + offset;
	offset += size;
	return size;
}

ubiq::GenericError MockFs::Stream::setPosition(Whence whence, int reqOffset)
{
	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::writeError();

	if(!file)
		return ubiq::GenericError::invalidArgumentError();

	int newOffset;
	switch(whence) {
		case Start:
			newOffset = reqOffset;
			break;
		case Current:
			newOffset = offset + reqOffset;
			break;
		case End:
			newOffset = file->data.size() + reqOffset;
			break;
	}

	if(newOffset < 0 || newOffset > file->data.size())
		return ubiq::GenericError::invalidSeekError();

	offset = newOffset;

	return true;
}

ubiq::GenericError MockFs::Stream::flush()
{
	if(!file)
		return ubiq::GenericError::invalidArgumentError();

	if(FsFailureSource::shouldFail())
		return ubiq::GenericError::writeError();

	return true;
}

unsigned int MockFs::Stream::getPosition() {
	return offset;
}

unsigned int MockFs::Stream::getSize()
{
	if(!file)
		return ubiq::GenericError::invalidArgumentError();

	return file->data.size();
}
