export CFLAGS = -g -ggdb3 -O0

export PKGS = eina elementary

default:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean
