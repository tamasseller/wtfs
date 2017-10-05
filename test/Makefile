OUTPUT = wtfs-test

# Pet sources

SOURCES += pet/ubiquitous/PrintfWriter.cpp
SOURCES += pet/test/MockAllocator.cpp

# Test code

#SOURCES += TestFrontConcurrent.cpp
SOURCES += TestBlobStress.cpp
SOURCES += TestBlobTree.cpp
SOURCES += TestBTreeKVTests.cpp
SOURCES += TestBTreeDateTests.cpp
SOURCES += TestBTreePoorHashTests.cpp
SOURCES += TestFrontGc.cpp
SOURCES += TestFrontIntegration.cpp
SOURCES += TestFrontMeta.cpp
SOURCES += TestFrontMount.cpp
SOURCES += TestFrontStream.cpp
SOURCES += TestMockFs.cpp
SOURCES += TestStorageBufferedStorage.cpp
SOURCES += TestStorageStorageManager.cpp
SOURCES += TestUtilPath.cpp
SOURCES += TestBTreeSimpleTests.cpp


# Test support

SOURCES += MockFs.cpp
SOURCES += TestMain.cpp
SOURCES += DateKey.cpp
SOURCES += KVTestKey.cpp
SOURCES += SimpleKey.cpp
SOURCES += PoorHashKey.cpp

# 1test

SOURCES += pet/1test/FailureInjector.cpp
SOURCES += pet/1test/TestRunner.cpp
SOURCES += pet/1test/TraceOutput.cpp
SOURCES += pet/ubiquitous/PrintfWriter.cpp

# Includes

INCLUDE_DIRS += .
INCLUDE_DIRS += ..
INCLUDE_DIRS += pet

# Flags

CXXFLAGS += -std=c++11								# Use the c++ standard released in 2011
CXXFLAGS += -O0 									# Avoid optimizer screwing up coverage data
CXXFLAGS += -g3										# Include all available debug info
CXXFLAGS += -fno-inline								# Do not inline (coverage again)
CXXFLAGS += -fmax-errors=1							# Avoid blowing the console
CXXFLAGS += -rdynamic								# Allows meaningful backrtaces 
CXXFLAGS += --coverage								# Generate program flow data and inject profiling code
CXXFLAGS += -fno-exceptions							# Enforce no-exceptions environment
CXXFLAGS += -fdelete-null-pointer-checks			# Makes branch coverage cleaner 
CXXFLAGS += -DCPPUTEST_USE_MEM_LEAK_DETECTION=0		# Turn off broken new operator macro magic in CppUTest
CXXFLAGS += -Wall									# Enable all warnings
CXXFLAGS += -Wno-unused								# Except for unused variables

LIBS += archive										# For the integration test

CXX=x86_64-linux-gnu-g++-6
LD=$(CXX) 

include ultimate-makefile/Makefile.ultimate	

enter:
	make -C toolchains enter-the-matrix CD_TO=../..
