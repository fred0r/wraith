#include "UpdateStreamTest.h"

#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>

#include "src/mod/update.mod/update_stream.h"

CPPUNIT_TEST_SUITE_REGISTRATION(UpdateStreamTest);

static bd::String make_payload(size_t length, char value = 'x')
{
  bd::String result;
  result.resize(length, value);
  return result;
}

void UpdateStreamTest::validChunk()
{
  bd::String decoded;

  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_OK,
      parse_update_chunk("8", "aGVsbG8=", decoded));
  CPPUNIT_ASSERT_EQUAL(bd::String("hello"), decoded);

  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_OK,
      parse_update_chunk("8", "AQIDBA==", decoded));
  CPPUNIT_ASSERT_EQUAL(bd::String("\x01\x02\x03\x04", 4), decoded);
}

void UpdateStreamTest::rejectsMissingChunk()
{
  bd::String decoded;

  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_MISSING,
      parse_update_chunk(NULL, "aaaa", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_MISSING,
      parse_update_chunk("", "aaaa", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_MISSING,
      parse_update_chunk("4", NULL, decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_MISSING,
      parse_update_chunk("4", "", decoded));
  CPPUNIT_ASSERT(decoded.isEmpty());
}

void UpdateStreamTest::rejectsBadSizes()
{
  bd::String decoded;

  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_SIZE,
      parse_update_chunk("3", "aGVsbG8=", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_SIZE,
      parse_update_chunk("9", "aGVsbG8=", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_SIZE,
      parse_update_chunk("-8", "aGVsbG8=", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_SIZE,
      parse_update_chunk("8x", "aGVsbG8=", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_SIZE,
      parse_update_chunk("8 ", "aGVsbG8=", decoded));
  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_SIZE,
      parse_update_chunk("999999999999999999999999999999",
          "aGVsbG8=", decoded));
  CPPUNIT_ASSERT(decoded.isEmpty());
}

void UpdateStreamTest::rejectsBadEncoding()
{
  bd::String decoded;

  CPPUNIT_ASSERT_EQUAL(UPDATE_CHUNK_BAD_ENCODING,
      parse_update_chunk("4", "!!!!", decoded));
  CPPUNIT_ASSERT(decoded.isEmpty());
}

void UpdateStreamTest::chunkBoundaries()
{
  bd::Stream stream;
  bd::String encoded;
  bool at_eof = false;

  CPPUNIT_ASSERT(next_update_chunk(stream, encoded, at_eof));
  CPPUNIT_ASSERT(at_eof);
  CPPUNIT_ASSERT(encoded.isEmpty());

  const bd::String exact = make_payload(1024);
  stream.clear();
  stream << exact;
  stream.seek(0, SEEK_SET);

  CPPUNIT_ASSERT(next_update_chunk(stream, encoded, at_eof));
  CPPUNIT_ASSERT(!encoded.isEmpty());
  CPPUNIT_ASSERT(at_eof);
  CPPUNIT_ASSERT_EQUAL(exact, bd::base64Decode(encoded));

  CPPUNIT_ASSERT(next_update_chunk(stream, encoded, at_eof));
  CPPUNIT_ASSERT(at_eof);
  CPPUNIT_ASSERT(encoded.isEmpty());
}

void UpdateStreamTest::multiChunkAssembly()
{
  const bd::String data = make_payload(1025, 'z');
  bd::Stream stream(data);
  bd::String encoded;
  bd::String assembled;
  bool at_eof = false;
  int chunks = 0;

  while (!stream.eof()) {
    CPPUNIT_ASSERT(next_update_chunk(stream, encoded, at_eof));
    CPPUNIT_ASSERT(!encoded.isEmpty());
    assembled += bd::base64Decode(encoded);
    ++chunks;
  }

  CPPUNIT_ASSERT_EQUAL(2, chunks);
  CPPUNIT_ASSERT(at_eof);
  CPPUNIT_ASSERT_EQUAL(data, assembled);
}
