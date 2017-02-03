OUTPUT = wtfs-test

# Pet sources

SOURCES += pet/ubiquitous/PrintfWriter.cpp
SOURCES += pet/test/MockAllocator.cpp

# Test code

#SOURCES += TestFrontConcurrent.cpp

SOURCES += TestBlobStress.cpp
SOURCES += TestBlobTree.cpp
SOURCES += TestBTreeDateTests.cpp
SOURCES += TestBTreeKVTests.cpp
SOURCES += TestBTreePoorHashTests.cpp
SOURCES += TestBTreeSimpleTests.cpp
SOURCES += TestFrontGc.cpp
SOURCES += TestFrontIntegration.cpp
SOURCES += TestFrontMeta.cpp
SOURCES += TestFrontMount.cpp
SOURCES += TestFrontStream.cpp
SOURCES += TestStorageBufferedStorage.cpp
SOURCES += TestStorageStorageManager.cpp
SOURCES += TestUtilPath.cpp
SOURCES += TestMockFs.cpp

# Test support

SOURCES += MockFs.cpp
SOURCES += TestMain.cpp
SOURCES += DateKey.cpp
SOURCES += KVTestKey.cpp
SOURCES += SimpleKey.cpp
SOURCES += PoorHashKey.cpp


# Failure injector

SOURCES += failure-injector/Addr2lineBacktrace.cpp
SOURCES += failure-injector/FailureInjectorPlugin.cpp
SOURCES += failure-injector/FailureInjectorShell.cpp
SOURCES += failure-injector/FailureSource.cpp
SOURCES += failure-injector/SimpleStringHelper.cpp
SOURCES += failure-injector/SourceRecord.cpp

# Includes

INCLUDE_DIRS += .
INCLUDE_DIRS += ..
INCLUDE_DIRS += pet
INCLUDE_DIRS += failure-injector

# Flags

CXXFLAGS += -std=c++11								# Use the c++ standard released in 2011
CXXFLAGS += -O0 									# Avoid optimizer screwing up coverage data
CXXFLAGS += -g3										# Include all available debug info
CXXFLAGS += -fno-inline								# Do not inline (coverage again)
CXXFLAGS += -m32									# Use 32 bit mode (to model the target MCUs)
CXXFLAGS += -fmax-errors=5							# Avoid blowing the console
CXXFLAGS += -rdynamic								# Allows meaningful backrtaces 
CXXFLAGS += --coverage								# Generate program flow data and inject profiling code
CXXFLAGS += -fno-exceptions							# Enforce no-exceptions environment
CXXFLAGS += -fdelete-null-pointer-checks			# Makes branch coverage cleaner 
CXXFLAGS += -DCPPUTEST_USE_MEM_LEAK_DETECTION=0		# Turn off broken new operator macro magic in CppUTest

LIBS += CppUTest									# Neat little unit testing framework 
LIBS += CppUTestExt									# Addition goodness, like mock support
LIBS += archive										# For the integration test

TEST_CMD_PREFIX=LD_LIBRARY_PATH=/opt/gcc-5.3.0/lib32/
CXX=/opt/gcc-5.3.0/bin/g++
LD=$(CXX) 

include ultimate-makefile/Makefile.ultimate	