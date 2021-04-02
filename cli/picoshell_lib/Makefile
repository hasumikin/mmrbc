TARGETS = shell.c

all: $(TARGETS)

shell.c: shell.rb
	../../build/host-debug/bin/picorbc -B shell -o $@ shell.rb

clean:
	rm -f $(TARGETS)
