Wandering Tree File System
==========================

Robust filesystem for **raw NAND flash** devices, with very small resource footprint, and high efficiency **zero-copy** data access semantics.
It is designed to use so little RAM that it can be deployed in contemporary, middle to high-end **MCU based systems** without external ram.
Efficient and robust due to its state-of-the-art wandering _B+ tree_ based storage structure.

Summary of goals
----------------

 - Tiny RAM foorptint
 - Designed-in robustness
 - High throughput
 - Usability in both single and multi-threaded (RTOS) environments.
 - No hard dependence on OS, compiler and standard libraries.
 - Ability to stay mounted with the use of a little non-volatile memory (ie backup ram or built-in flash) in low-power situations.
 
It was also a secondary intention to make use of modern compiler technology to enhance the overall system 
performance by leveraging agressive inlining. 
And also to introduce internal decoupling, through templatization to make the whole software module
thoroughly testable layer by layer.

![System block diagram](https://g.gravizo.com/g?
  digraph G {
    wtfs[shape=record label="<l>wtfs\\nlogic|<b>buffers|<m>manager"];
    app[label = "Application"];
    drv[label = "Driver"];
    edge[penwidth=3]
    app -> wtfs:b [label = "Direct\\naccess"];
    wtfs:b -> drv[label = "DMA"];
    edge[penwidth="" style=dashed]
    app -> wtfs:l [label = "Meta\\nops"];
    wtfs:m -> drv[label = "Commands"];
  }
)

How to use it
-------------

**To use this library you need a decent compiler with stable C++11 support** (like GCC 5+)

The structure of the source tree is segmented into directories based on topic, 
but there is significant cross including going on between the headers, so to keep
things simple **add the root of the libraray tree to your include path** to get things going.

The whole filesystem code is implemented as set of class templates and all of the configuration data and
environment dependencies are injected through a single template parameter.

The actual instantiation of the fs is done in two parts to decouple actual system state from buffering area. 
This is required so that the main fs data (ie. the part that is not the buffers) can be kept in "backup ram" or saved to on-board flash.
Which in turn alleviates mounting overhead (both time and power), which is a rather happy thing for battery powered devices that often
go through a sleep-wake cycles.	This also adds more flexibility to the placement of the static memory blocks used by the fs as only 
the buffer part needs to be accessible for the DMA (if used).

Here is an example of the minimal fs instantiation and system start up code:

```c++
#include "Wtsf.h"

typedef Wtfs<Config> Fs;

Fs fs;
Fs::Buffers fsBuffers;

int main()
{
	fs.bind(&fsBuffers);
	fs.initialize();

	// do stuff
}
```

_User code only ever needs to include header the file called 'Wtfs.h' in the root of the library tree._

The application facing software interface (API) of the filesystem is rather unconventional, which is justified by the fact that this
way it is possible to allocate all the memory required declaratively - that is, without the need for dynamic memory allocation.
As parsing path strings and iterating through the entries of the filesystem is not strictly the job of the fs itself, these 
operations are only available as optional helpers.

The low-level api involves two types nested into the the fs type: 

 - _Node_: represents a file or directory entry in the filesystem but not the data associated with it. 
   It can be used as an iterator object to traverse the filesystem, either for listing contents or for finding an entry with a given name.
   It can also be used for opening the file data as Streams.
 - _Stream_: represents the contents of a given file.
   It has a zero-copy enabled interface - that is, its read and write operations do not copy the data, but rather they give the application 
   the buffers that are needed to be read from or written to.
 
See **code examples below** or the [docs](http://???) for the detailed method descriptions.

Names are not necessarily standard C (ie. zero-terminated) strings, to avoid wasting the last byte of the file name field in the stored
data structures. It can matter if you want to go extremely dense, and use very short filenames.

There are several helpers that establish a higher-level, more familiar view and makes it more convenient to use the system for common use-cases:

 - Path: represents a path, with internal identifiers. Also can parse textual paths.
 - ObjectStream: a decorator template for the Stream object that adds copier methods.

See **code examples below** or the [docs](http://???) for the detailed method descriptions.

### Configuration

To acheive high performance and wide usability at the same time with a single, well tested code base it is necessary 
to leave many things configurable for the user. 

As stated above, the dependency injection is implemented in the form of a single template argument.
This is a rather unorthodox method of supplying configuration data, compared to the plain-old &lt;something&gt;_config.h
method, but it has several advantages:

 - All of the application, the fs code, and the injected dependencies are available for the compiler for inlining.
 - Makes it easy to find the boundaries between the fs code and the glue.
 - Makes it possible to compose the configuration from classes implementing different aspects, 
   through subclassing or by employing the using keyword.

The configuration is provided as template type argument, this type - supplied by the user - has to contain several constants and nested types.
There are many concerns that the configuration type has to address, like: low-level I/O access driver, methods of dynamic memory allocation,
and locking if used with an OS, also some tweakable parameters are exposed this way.

See **code examples below** or the [docs](http://???) for the detailed descriptions.

### Code examples

A file named foo can be created in the root like so:
```c++
Fs::Node node;

const char* name = "foo";

fs.fetchRoot(node);
fs.newFile(node, name, name+strlen(name));
```

A file at /bar/foo can be opened like this:
```c++
Fs::Node node;
Fs::Stream stream;

const char *foo = "foo", *bar = "bar";

fs.fetchRoot(node);
fs.fetchChildByName(node, bar, bar+strlen(bar));
fs.fetchChildByName(node, foo, foo+strlen(foo));
fs.openStream(node, stream);
```

The contents of directory /foobar can be listed like so:
```c++
Fs::Node node;
Fs::Stream stream;

const char *foobar = "foobar";

fs.fetchRoot(node);
fs.fetchChildByName(node, foobar, foobar+strlen(foobar));

for(fs.fetchFirstChild(node); fs.fetchNextSibling(node);) {
	const char *start, *end;
	node.getName(start, end);
	fwrite(start, 1, end-start, stdout);
}
```

Printing a file named "/baz":
```c++
unsigned int n;
void* bufferPointer;

fs.openStream(node, stream);

while(n = stream.read(bufferPointer, 1024))
	fwrite(bufferPointer, 1, n, stdout);
	
fs.closeStream(stream);
```

_Note that the length specified for read is just a maximum, it is quite possible that
it will return a smaller number meaning that the buffer has less data that what was asked for.
It returns non-positive value only if there is no more data to be read (EOF or error), otherwise it blocks._

Filling a file named "/null" with 3141 zeroes:
```c++
unsigned int n;
void* bufferPointer;

fs.openStream(node, stream);

for (int l = 3141; n = stream.write(bufferPointer, l); l -= n)
	memset(bufferPointer, 0, n);

fs.closeStream(stream);
```

_Note that it may seem counter-intuitive that the write method is called first and the data is filled in afterwards, 
but remember that the write method only returns the buffer needed for writing, the subsequent calls to write or
close are the ones that actually write the data out, if required by the internal buffer scheduling._

_Also the length parameter of write is only a maximum, just as with the read method_

A schematic example is provided of a minimal single-threaded bare-metal configuration.

```c++
#include "Wtfs.h"

struct Config: public DefaultNolockConfig {
	static constexpr unsigned int nBuffers = 7;				// Number of page buffers.
	static constexpr unsigned int maxMeta = 5;              // Maximum number of meta tree levels. (see below)
	static constexpr unsigned int maxFile = 5;				// Maximum number of file tree levels. (see below)
	static constexpr unsigned int maxFilenameLength = 47;	// Maximum length of file and direvtory names in bytes.

	struct FlashDriver {
		typedef unsigned int Address;						// A type that is capable of holding a _page_ address.
		static constexpr unsigned int InvalidAddress=-1u;	// An address that can not be accessed (used like NULL but for the storage).
		static constexpr unsigned int pageSize = 2112;		// The size of a single page in bytes.
		static constexpr unsigned int blockSize = 64;		// The number of pages in a block.
		static constexpr unsigned int deviceSize = 2048;	// The number of blocks in the device

		/* This method is called by the upper layers when a block erase is required.
		 * It is eventually called on already erased blocks, but only if some kind of 
		 * error handling is in progress. So it may decide to do nothing if it has some
		 * sensible way of determining if a block is already clean. */
		static void ensureErased(unsigned int blockAddress) {
			// do it (depends on hw)
		}

		/* This method is called by the upper layers when a block is to be read. */
		static void read(Address addr, void* data) {
			// do it (depends on hw)
		}

		/* This method is called by the upper layers when a block is to be written. */
		static void write(Address addr, void* data) {
			// do it (depends on hw)
		}
	};

	struct Allocator {
		/* Malloc, but you are free to provide anything that, does the job. */
		static void* alloc(unsigned int s) {
			// do it (malloc if you have it)
		}

		/* Same as free, but you are free to provide anything that, does the job. */
		static void free(void* p) {
			// do it (free if you have it)
		}
	}
};
```

How it works
------------

The structure of the stored data is based on a - rather fancy - wandering B+ tree aproach, 
the very same technique that most database software and the spectacular linux btrfs is based on.
So that means that all update operations have to write a new version of all touched nodes, 
which is a good fit for NAND flash, because you can not really update anything in place due 
to the erase-then-write nature of it anyway.
Because that update procedure includes the root of the tree, the whole structure has a single
"atomically" writable entry point, thus making it transactionally updatable.

Also the b+ tree is very efficient for both inserting and searching, so by basing the metadata 
storage on the b+ tree, two of the goals are automatically met: robustness and efficiency.
The efficiency gained by employing it can then be traded off for minimizing the ram usage by
keeping only the absolutely necessary amount of stored data in RAM.

That is achieved by implementing the tree in a way that pages containing the nodes are read as late
as possible, and discarded as soon as they can be. 
This could have lead to terrible inefficiency, but to counteract this a cached page buffer have been
put between the tree management module and the actual media access layer.

However because the upper layers of the b+ tree contain only very minimal amount of data (about 12 bytes per child)
a very high fanout is achieved (~50-300 depending on pages size and ECC parameters), which means that the
meta tree is rather shallow even for large number of stored files.
So a modest number of page buffers (below ten for light concurrency) can totally avoid the
unnecessary I/O problem of tree operations - going for a higher number of buffers does not add any further speedup.

Note that this caching/buffering layer has to do writebacks of the dirty data in the correct order of accesses,
to preserve the above mentioned goodnesses of transactional tree updates. 
It also provides a method to flush the current contents to enable the user to ensure completion 
of operations in critical application processes.

For a more in-depth description see the [docs](http://???).

Implementation notes
--------------------

### Dynamic memory

The use of dynamic memory, in algorithms of this complexity is nearly inevitable, but it is kept at minimum as heap usage is big no-no
for deeply embedded, always-on system. So, as little as a few hundred bytes of heap should do it, for the intended use-cases.

Future plans
------------

 - Support multiple flash devices.
 - I/O queuing.
 - Optional additional metadata, like timestamps or access flags.
 - Application hooks to update/use that metadata.
