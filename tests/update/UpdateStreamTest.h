#ifndef TESTS_UPDATE_UPDATESTREAMTEST_H
#define TESTS_UPDATE_UPDATESTREAMTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class UpdateStreamTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(UpdateStreamTest);
  CPPUNIT_TEST(validChunk);
  CPPUNIT_TEST(rejectsMissingChunk);
  CPPUNIT_TEST(rejectsBadSizes);
  CPPUNIT_TEST(rejectsBadEncoding);
  CPPUNIT_TEST(chunkBoundaries);
  CPPUNIT_TEST(multiChunkAssembly);
  CPPUNIT_TEST_SUITE_END();

public:
  void validChunk();
  void rejectsMissingChunk();
  void rejectsBadSizes();
  void rejectsBadEncoding();
  void chunkBoundaries();
  void multiChunkAssembly();
};

#endif
