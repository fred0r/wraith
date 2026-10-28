ROOT := ..
CXX ?= c++
CXXFLAGS ?= -g -O2
LDFLAGS ?=
LIBS ?=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
ASAN_OPTIONS ?= detect_leaks=1
else
ASAN_OPTIONS ?=
endif

CPPUNIT_CFLAGS := $(shell pkg-config --cflags cppunit 2>/dev/null)
CPPUNIT_LIBS := $(shell pkg-config --libs cppunit 2>/dev/null)
CRYPTO_LIBS := $(shell pkg-config --libs libcrypto 2>/dev/null)
ifeq ($(strip $(CRYPTO_LIBS)),)
CRYPTO_LIBS := -lcrypto
endif

BDLIB := $(ROOT)/lib/bdlib/libbdlib.a
INCLUDES := -I$(ROOT) -I$(ROOT)/src -I$(ROOT)/lib -I$(ROOT)/lib/bdlib
DEFS := -DHAVE_CONFIG_H -DOPENSSL_API_COMPAT=0x10000000L -DGENERATING_DEFS
PROJECT_CPPFLAGS := $(INCLUDES) $(DEFS) $(CPPUNIT_CFLAGS)
PROJECT_CXXFLAGS := $(filter-out -fno-rtti,$(CXXFLAGS)) -Wno-deprecated-declarations

TEST_SOURCES := \
	UnitRunner.cpp \
	support/Stubs.cpp \
	base64/Base64Test.cpp \
	crypto/CryptoTest.cpp \
	update/UpdateStreamTest.cpp \
	link/EnclinkTest.cpp \
	cookie/CookieCodecTest.cpp \
	cookie/CookieDifferentialTest.cpp \
	cookie/LegacyCookieReference.cpp \
	pending_cookies/PendingCookieTest.cpp \
	pending_cookies/PendingCookieDifferentialTest.cpp \
	pending_cookies/LegacyPendingCookieQueue.cpp

PROD_SOURCES := \
	$(ROOT)/src/base64.cc \
	$(ROOT)/src/crypto/aes_util.cc \
	$(ROOT)/src/crypto/bf_util.cc \
	$(ROOT)/src/crypto/chacha_util.cc \
	$(ROOT)/src/crypto/dh_util.cc \
	$(ROOT)/src/mod/update.mod/update_stream.cc \
	$(ROOT)/src/enclink_util.cc \
	$(ROOT)/src/mod/irc.mod/cookie_codec.cc \
	$(ROOT)/src/mod/irc.mod/pending_cookies.cc \
	$(ROOT)/src/rfc1459.cc \
	$(ROOT)/src/sprintf.cc

TEST_OBJS := $(patsubst %.cpp,build/tests/%.o,$(TEST_SOURCES))
PROD_OBJS := $(patsubst $(ROOT)/src/%.cc,build/src/%.o,$(PROD_SOURCES))
ASAN_TEST_OBJS := $(patsubst %.cpp,build-asan/tests/%.o,$(TEST_SOURCES))
ASAN_PROD_OBJS := $(patsubst $(ROOT)/src/%.cc,build-asan/src/%.o,$(PROD_SOURCES))
UBSAN_TEST_OBJS := $(patsubst %.cpp,build-ubsan/tests/%.o,$(TEST_SOURCES))
UBSAN_PROD_OBJS := $(patsubst $(ROOT)/src/%.cc,build-ubsan/src/%.o,$(PROD_SOURCES))
SANITIZE_FLAGS := -fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer
UBSAN_FLAGS := -fsanitize=undefined -fno-sanitize-recover=all -fno-sanitize=vptr -fno-omit-frame-pointer

.PHONY: all test test-asan test-ubsan clean check-cppunit

all: UnitRunner

test: check-cppunit UnitRunner
	./UnitRunner

test-asan: check-cppunit UnitRunner-asan
	ASAN_OPTIONS="$(ASAN_OPTIONS)" ./UnitRunner-asan

test-ubsan: check-cppunit UnitRunner-ubsan
	./UnitRunner-ubsan

check-cppunit:
	@pkg-config --exists 'cppunit >= 1.14.0' || \
		{ echo "cppunit >= 1.14.0 is required for tests (libcppunit-dev)"; exit 1; }

UnitRunner: $(TEST_OBJS) $(PROD_OBJS) $(BDLIB)
	$(CXX) $(LDFLAGS) -o $@ $(TEST_OBJS) $(PROD_OBJS) $(BDLIB) \
		$(CPPUNIT_LIBS) $(CRYPTO_LIBS) $(LIBS) -ldl

UnitRunner-asan: $(ASAN_TEST_OBJS) $(ASAN_PROD_OBJS) $(BDLIB)
	$(CXX) $(LDFLAGS) $(SANITIZE_FLAGS) -o $@ $(ASAN_TEST_OBJS) $(ASAN_PROD_OBJS) \
		$(BDLIB) $(CPPUNIT_LIBS) $(CRYPTO_LIBS) $(LIBS) -ldl

UnitRunner-ubsan: $(UBSAN_TEST_OBJS) $(UBSAN_PROD_OBJS) $(BDLIB)
	$(CXX) $(LDFLAGS) $(UBSAN_FLAGS) -o $@ $(UBSAN_TEST_OBJS) $(UBSAN_PROD_OBJS) \
		$(BDLIB) $(CPPUNIT_LIBS) $(CRYPTO_LIBS) $(LIBS) -ldl

$(BDLIB):
	+@$(MAKE) -C $(ROOT)/lib/bdlib

build/tests/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PROJECT_CPPFLAGS) $(PROJECT_CXXFLAGS) -c $< -o $@

build/src/%.o: $(ROOT)/src/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(PROJECT_CPPFLAGS) $(PROJECT_CXXFLAGS) -c $< -o $@

build-asan/tests/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PROJECT_CPPFLAGS) $(PROJECT_CXXFLAGS) $(SANITIZE_FLAGS) -c $< -o $@

build-asan/src/%.o: $(ROOT)/src/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(PROJECT_CPPFLAGS) $(PROJECT_CXXFLAGS) $(SANITIZE_FLAGS) -c $< -o $@

build-ubsan/tests/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PROJECT_CPPFLAGS) $(PROJECT_CXXFLAGS) $(UBSAN_FLAGS) -c $< -o $@

build-ubsan/src/%.o: $(ROOT)/src/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(PROJECT_CPPFLAGS) $(PROJECT_CXXFLAGS) $(UBSAN_FLAGS) -c $< -o $@

clean:
	rm -rf build build-asan build-ubsan UnitRunner UnitRunner-asan UnitRunner-ubsan
