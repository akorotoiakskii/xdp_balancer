CLANG ?= clang
LLC ?= llc

CFLAGS = \
	-I libbpf/src/root/usr/include \
	-fno-stack-protector -O2 -g \
	-O2 -g -c -target bpf

xdp_%.o: xdp_%.c Makefile
	$(CLANG) -c -emit-llvm $(CFLAGS) $< -o - | \
	$(LLC) -march=bpf -filetype=obj -o $@

.PHONY: all clean

all: xdp_pass.o xdp_redirect.o

clean:
	rm -f ./*.o
