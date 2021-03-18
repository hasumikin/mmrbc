CFLAGS += -Wall -Wpointer-arith -std=gnu99
AR := ar
CC := gcc
TARGET := libpicorbc.a
SRCS := compiler.c common.c token.c tokenizer.c generator.c my_regex.c scope.c node.c stream.c
OBJS := $(SRCS:%.c=%.o)
DEPS := $(SRCS:%.c=%.d)

all: tokenizer_helper.h ruby-lemon-parse/parse.c regex_light/src/regex.c $(TARGET)

-include $(DEPS)

regex_light/src/regex.o:
ifeq ($(findstring MRBC_ALLOC_LIBC, $(CFLAGS)), MRBC_ALLOC_LIBC)
	cd regex_light/src ; $(MAKE) regex.o CFLAGS="-DREGEX_USE_ALLOC_LIBC $(CFLAGS)"
else
	cd regex_light/src ; $(MAKE) regex.o
endif

tokenizer_helper.h: token.h tokenizer.h
	./helper_gen.rb tokenizer

ruby-lemon-parse/parse.c:
	cd ruby-lemon-parse ; \
	  LEMON_MACRO=-DLEMON_PICORBC $(MAKE) parse.c atom_helper.h token_helper.h keyword_helper.h

$(TARGET): $(OBJS) regex_light/src/regex.o
ifeq ($(findstring PICORUBY_REGEX_LIBC, $(CFLAGS)), PICORUBY_REGEX_LIBC)
	$(AR) $(ARFLAGS) $@ $(filter-out regex_light/src/regex.o, $^)
else
	$(AR) $(ARFLAGS) $@ $^
endif

%.o: %.c
	cd include ; $(MAKE) all CC=$(CC)
	$(CC) -c -MMD -MP $(CFLAGS) $(LDFLAGS) $<

clean:
	cd regex_light/src ; $(MAKE) clean
	cd ruby-lemon-parse ; $(MAKE) clean
	cd mrubyc/src ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS) $(DEPS) tokenizer_helper.h
