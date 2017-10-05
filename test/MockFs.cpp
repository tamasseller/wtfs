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
#include <cstring>

#include "1test/FailureInjector.h"

MockFs::Entry::Entry(const std::string &name, struct Directory* parent):
	name(name), parent(parent)
{
	if(parent)
		parent->children.push_back(this);
}

pet::GenericError MockFs::fetchRoot(Node& node)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	node.entry = &root;
	return true;
}

pet::GenericError MockFs::fetchChildByName(Node& node, const char* start, const char* end)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	if(!node.entry->isDir())
		return pet::GenericError::isNotDirectory;

	if(!end)
		end = start + std::strlen(start);

	std::string name(start, end-start);
	for(auto x: ((Directory*)node.entry)->children) {
		if(x->name == name) {
			node.entry = x;
			return true;
		}
	}

	return pet::GenericError::noSuchEntryError();
}

pet::GenericError MockFs::fetchChildById(Node& node, NodeId id)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	if(!node.entry->isDir())
		return pet::GenericError::isNotDirectory;

	for(auto x: ((Directory*)node.entry)->children) {
		if((uintptr_t)x == id) {
			node.entry = x;
			return true;
		}
	}

	return pet::GenericError::noSuchEntryError();
}

pet::GenericError MockFs::fetchFirstChild(Node& node)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	if(!node.entry->isDir())
		return pet::GenericError::isNotDirectoryError();

	auto &children = ((Directory*)node.entry)->children;
	auto x = children.begin();

	if(x == children.end())
		return false;

	node.entry = *x;
	return true;
}

pet::GenericError MockFs::fetchNextSibling(Node& node)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	if(!node.entry->parent) // the root has no siblings
		return false;

	auto &children = node.entry->parent->children;
	auto x = std::find(children.begin(), children.end(), node.entry);

	if(x == children.end()) // current not found, WTF ??
		return pet::GenericError::noSuchEntryError();

	++x;

	if(x == children.end()) // last child
		return false;

	node.entry = *x;
	return true;
}

pet::GenericError MockFs::checkNew(Node& node, const char* start, const char* end)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	std::string name(start, end-start);
	for(auto x: ((Directory*)node.entry)->children)
		if(x->name == name)
			return pet::GenericError::alreadyExistsError();

	return true;
}

pet::GenericError MockFs::newDirectory(Node& node, const char* start, const char* end)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::writeError();

	pet::GenericError ret = checkNew(node, start, end);
	if(ret.failed())
		return ret.rethrow();

	node.entry = new Directory(std::string(start, end-start), (Directory*)node.entry);
	return true;
}

pet::GenericError MockFs::newFile(Node& node, const char* start, const char* end)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::writeError();

	pet::GenericError ret = checkNew(node, start, end);
	if(ret.failed())
		return ret.rethrow();

	node.entry = new File(std::string(start, end-start), (Directory*)node.entry);
	return true;
}

pet::GenericError MockFs::removeNode(Node& node)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::writeError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	if(!node.entry->parent) // cannot remove root
		return pet::GenericError::invalidArgumentError();

	if(node.entry->isDir() && !((Directory*)node.entry)->children.empty())
		return pet::GenericError::notEmptyError();

	auto &children = node.entry->parent->children;
	auto x = std::find(children.begin(), children.end(), node.entry);

	if(x == children.end())
		return pet::GenericError::noSuchEntryError();

	children.erase(x);

	return true;
}

pet::GenericError MockFs::openStream(Node& node, Stream& stream)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!node.entry)
		return pet::GenericError::invalidArgumentError();

	if(node.entry->isDir())
		return pet::GenericError::isDirectoryError();

	stream.file = (File*)node.entry;
	return true;
}

pet::GenericError MockFs::flushStream(Stream&) {
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

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

pet::GenericError MockFs::Stream::read(void* &content, unsigned int size)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::readError();

	if(!file)
		return pet::GenericError::invalidArgumentError();

	if(offset >= file->data.size())
		return 0;

	content = file->data.data() + offset;

	if(offset + size > file->data.size())
		return file->data.size() - size;

	offset += size;
	return size;
}

pet::GenericError MockFs::Stream::write(void* &content, unsigned int size)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::writeError();

	if(!file)
		return pet::GenericError::invalidArgumentError();

	while(file->data.size() < offset + size)
		file->data.push_back('\0');

	content = file->data.data() + offset;
	offset += size;
	return size;
}

pet::GenericError MockFs::Stream::setPosition(Whence whence, int reqOffset)
{
	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::writeError();

	if(!file)
		return pet::GenericError::invalidArgumentError();

	unsigned int newOffset;
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
		return pet::GenericError::invalidSeekError();

	offset = newOffset;

	return true;
}

pet::GenericError MockFs::Stream::flush()
{
	if(!file)
		return pet::GenericError::invalidArgumentError();

	if(pet::FailureInjector::shouldSimulateError())
		return pet::GenericError::writeError();

	return true;
}

unsigned int MockFs::Stream::getPosition() {
	return offset;
}

unsigned int MockFs::Stream::getSize()
{
	if(!file)
		return pet::GenericError::invalidArgumentError();

	return file->data.size();
}
