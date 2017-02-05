For 1.0
========

Cleanup
-------

 - Add default value for second argument of fetchChildByName, that enables it to be used conveniently on C strings.
 - Add copyName helper into Node, to ease its use with pure C strings (via some kind of utility decorator CRTP stuff).
 - Check with -m32 and -m64
 - -Wall proofing all tests (add -Wall -Werror to the global makefile)
 - On HW test suite (big pseudo random data, many files, funny directory structures).
 - Config static_assert sanity checks.

Implementation
--------------

 - Enable FlashDriver to return errors (ok, corrected, not correctable)
 - Update README example code.
 - Propagate low-level errors from BufferedStorage.
 - Add bad block discrimination logic (failing status is not needed as a full sized bitmap, it is enough to track a few failing blocks only).
 - Implement bad block marking/detection to the driver interface.
 - Implement bad block avoidance in block allocation logic.
 - Add sw ECC logic as driver helper.
 - Add erase session variant that does not do any logging (as it supports only erase and that can not fail)
 - Fix memory runaway on deleting large file.
 - Add non deleting truncate-to-zero API.
 - Add configuration hash generation (one byte is enough)
 - Add check byte masked with the config hash, to avoid crashing on random data.
 - Add give-back method to Stream (for partially read/written buffers)
 - Implement unstable state double (or triple) checking in mount logic.
 
Testing
-------

 - Make the MockFlashDriver failable.
 - Error handling tests in gc.
 - Error handling tests in mounting.
 - ECC tests.
 - Artifically induced bit error tests
 - Unstable state (interrupted erase/write) simulations
 
Documentation
-------------

 - General concept overview 
   - fs: non-unix, tree, uniqueness (Node + Stream, example Linux vs Windows)
   - zero-copy 
 - API (doxy)
   - Node
   - Stream
   - Mounting
 - Driver interface  
 - Hacking support docs:
   - Design considerations: graphs of the whole tree.
   - Level numbering
   - Allocation scheme
   - Btree (doxy + theory and graphs for operations)
   - Blobtree (doxy)
   - Caching, (doxy + coherency)
   - Storage, (doxy + transaction management)


For 2.0
========

Implementation
--------------

 - Implement waiting  for individual buffers, instead of waiting for the current transfer to complete.
 - Transform driver interface and BufferedStorage blocking scheme to enable queuing of I/O requests.
 - Separate reader-writer locking of meta and file tree operations (as the whole point of the Node thing is to separate those).
 - Support optional additional metadata (like timestamps, or access flags) 
 - Add application hooks to update/use the metadata.
 - Prefetch operation in BufferedStorage.
 - Prefetch hinting in BlobTree for file read operations.
