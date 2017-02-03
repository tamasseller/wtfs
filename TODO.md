Cleanup
-------

 - Check with -m32 and -m64
 - -Wall proofing all tests (add -Wall -Werror to the global makefile)
 - On HW unit tests.

Implementation
--------------

 - Enable FlashDriver to return errors (ok, corrected, not correctable)
 - Update README example code.
 - Propagate low-level errors from BufferedStorage.
 - Add sw ECC logic as driver helper.
 - Add erase session variant that does not do any logging (as it supports only erase and that can not fail)
 - Fix memory runaway on deleting large file.
 - Add non deleting truncate-to-zero API.
 
Testing
-------

 - Make the MockFlashDriver failable.
 - Error handling tests in gc.
 - Error handling tests in mounting.
 
Documentation
-------------

 - General concept overview 
   - fs: non-unix, tree, uniqueness (Node + Stream, example Linux vs Windows)
   - zero-copy 
 - API (doxy)
   - Node
   - Stream
   - mounting
 - Driver interface  
 - Hacking support docs:
   - design considerations, btree, graphs for structure.
   - Caching, (doxy + coherency)
   - Storage, (doxy + transaction management)
   - Btree (doxy + theory and graphs for operations)
   - Blobtree (doxy)



