Wandering Tree File System
==========================

Robust filesystem for *raw NAND flash* devices, witch very small resource footprint, and high efficiency zero-copy data access semantics.
It is designed to use so little RAM that it can be deployed in contemporary, middle to high-end *MCU based systems* without external ram.
Efficient and robust due to its state-of-the-art wandering B+ tree based storage structure.

Summary of goals
----------------

 - Tiny RAM foorptint
 - Designed-in robustness
 - High throughput
 - Usability in both single and multi-threaded (RTOS) environments.
 - No hard dependence on OS, compiler and standard libraries.
 - Ability to stay mounted with the use of a little non-volatile memory (ie backup ram or built-in flash) in low-power situations.
 
It was also a secondary intention to make use of modern compiler technology to enhance the overall system 
performance enabled by agressive inlining. 
And also to employ internal dependency injection, thorough templatization to make the whole software module
thoroughly testable layer by layer.

So in order *to use this library you need a decent compiler with stable C++11 support* (like GCC 5+)

How to use it
-------------

To acheive high performance and wide usability at the same time with the same code base it is necessary to
leave many things configurable for the integrator. 

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

The whole is filesystem code is implemented as set of class templates and all of the configuration data and
environment dependencies are injected through a single template parameter.
This is a rather unorthodox method of configuration compared to the usual <somthig>_config.h method, but it
has several advantages:

 - All of the application, the fs code, and the injected dependencies are available for the compiler for inlining.
 - This arrangement makes it easy to find the boundaries between the fs code and the glue.
 - Makes it possible to compose the configuration from classes implementing different aspects, 
   through subclassing or by employing the using keyword.

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

The actual instantiation of the fs is done in two parts to facilitate the decoupling of actual system state from buffer contents. 

NOTE:
	It is required so that the main fs data (ie. the part that is not the buffers) can be kept in "backup ram" or saved to on-board flash.
	Which in turn alleviates mounting overhead (both time and power), which is a rather happy thing for battery powered devices that often
	go through a sleep-wake cycle.
	
NOTE:
	This also adds more flexibility to the placement of the static memory blocks used by the fs as only the buffer part needs to be
	accessible for the DMA (if used).

Here is an example of the minimal file instantiation and system start up code:

```c++
typedef Wtfs<WtfsConfig> Fs;

Fs fs;
Fs::Buffers fsBuffers;

int main()
{
	fs.bind(&fsBuffers);
	fs.initialize();

	// do stuff
}
```

The application facing software interface (API) of the filesystem is rather unconventional, which is justified by the fact that this
way it is possible to allocate all the memory required declaratively - that is, without the need for dynamic memory allocation.
As parsing path strings and iterating through the contents of the fs is not strictly the job of the fs itself, these operations are only 
available as optional helpers. 

The low-level api involves two types nested into the the fs type: 

 - _Node_: represents a file or directory entry in the filesystem but not the data associated with it. 
   It can be used as a special iterator to traverse the filesystem, either for listing contents or for finding an entry with a given name.
   It can also be used for opening the file data as Streams.
 - _Stream_: represents the contents of a given file.
   It has a zero-copy enabled interface - that is, its read and write operations do not copy the data, but rather they give the application 
   the buffers that are needed to be read from or written to.
 
See the [docs](http://???) for the detailed method descriptions.

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

NOTE
	Names are not necessarily standard C (ie. zero-terminated) strings, in order to 
	avoid wasting the last byte of the file name field in the stored data structures. 
	It can matter if you want to go extremely dense, and use very short filenames.

There are several helpers that establish a higher-level, more familiar view and makes it more convenient to use the system for common use-cases:

 - Path: represents a path, by internal identifier. Also can parse textual paths.
 - ObjectStream: a decorator template for the Stream object that adds copier methods.

See the [docs](http://???) for the detailed method descriptions.

How it works
------------

The structure of the stored data is based on a wandering B+ tree. 
All update operations have to write a new version of the root, which makes
This means that the whole structure has a single "atomically" writable entry point. 

TODO

Implementation notes
--------------------

### Dynamic memory

The use of dynamic memory, in algorithms of this complexity is nearly inevitable, but it is kept at minimum as heap usage is big no-no
for deeply embedded, always-on system. So as little as a few hundred bytes of heap should do it, for regular use-cases.

Future plans
------------

 - Support multiple flash devices.
 - Transform driver interface and blocking scheme to enable queuing of I/O requests,
   and thus wait for individual buffers, instead of waiting for the current transfer to complete.
 - Support optional additional metadata (like timestamps, or access flags) and application hooks
   to update/use it.
