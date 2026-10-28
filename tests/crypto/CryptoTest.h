#ifndef TESTS_CRYPTO_CRYPTOTEST_H
#define TESTS_CRYPTO_CRYPTOTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class CryptoCipherTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(CryptoCipherTest);
  CPPUNIT_TEST(aesEcbKnownVector);
  CPPUNIT_TEST(aesRoundtrip);
  CPPUNIT_TEST(blowfishRoundtrip);
  CPPUNIT_TEST(blowfishEcbKnownVector);
  CPPUNIT_TEST(blowfishCbcKnownVector);
  CPPUNIT_TEST(blowfishBlockBoundaries);
  CPPUNIT_TEST(blowfishRejectsMalformed);
  CPPUNIT_TEST(blowfishCbcRejectsMalformed);
  CPPUNIT_TEST(blowfishEmptyKey);
  CPPUNIT_TEST(chachaRoundtrip);
  CPPUNIT_TEST(chachaKeyLengths);
  CPPUNIT_TEST(chachaRejectsTampering);
  CPPUNIT_TEST(chachaRejectsMalformedBase64);
  CPPUNIT_TEST_SUITE_END();

public:
  void aesEcbKnownVector();
  void aesRoundtrip();
  void blowfishRoundtrip();
  void blowfishEcbKnownVector();
  void blowfishCbcKnownVector();
  void blowfishBlockBoundaries();
  void blowfishRejectsMalformed();
  void blowfishCbcRejectsMalformed();
  void blowfishEmptyKey();
  void chachaRoundtrip();
  void chachaKeyLengths();
  void chachaRejectsTampering();
  void chachaRejectsMalformedBase64();
};

class DH1080Test : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(DH1080Test);
  CPPUNIT_TEST(fishBase64Roundtrip);
  CPPUNIT_TEST(sharedSecretSymmetry);
  CPPUNIT_TEST(rejectsInvalidPeerKey);
  CPPUNIT_TEST_SUITE_END();

public:
  void fishBase64Roundtrip();
  void sharedSecretSymmetry();
  void rejectsInvalidPeerKey();
};

#endif
