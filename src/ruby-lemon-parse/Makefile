CC := gcc
PARSE := parse

all: $(PARSE).o keyword_helper.h token_helper.h

atom_helper.h: parse_header.h
	./helper_gen.rb atom

keyword_helper.h: parse.h
	./helper_gen.rb keyword

token_helper.h: parse.h
	./helper_gen.rb token

build_so: lib$(PARSE).so

$(PARSE).o: $(PARSE).c atom_helper.h 
	$(CC) $(CFLAGS) $(LEMON_MACRO) $(LDFLAGS) -c -o $(PARSE).o $(PARSE).c

lib$(PARSE).so: $(PARSE).c
	$(CC) $(CFLAGS) $(LEMON_MACRO) -fPIC -shared $(LDFLAGS) -o lib$(PARSE).so $(PARSE).c

$(PARSE).c: $(PARSE).y lemon/lemon
	./lemon.rb $(LEMON_MACRO)

lemon/lemon:
	cd lemon ; $(MAKE) all CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

clean:
	cd lemon ; $(MAKE) clean
	rm -f $(PARSE).c lib$(PARSE).so $(PARSE).h $(PARSE).out $(PARSE).o \
	  *_helper.h
