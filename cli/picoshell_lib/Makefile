TARGETS = shell.c

all: $(TARGETS)

shell.c: shell.rb
	mrbc -E -Bshell -o $@ shell.rb

clean:
	rm -f $(TARGETS)
