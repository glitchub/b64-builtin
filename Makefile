# Where did "apt install bash-builtins" put the headers?
HEADERS = /usr/include/bash

CPPFLAGS = -I${HEADERS} -I${HEADERS}/include -I${HEADERS}/builtins

CFLAGS += -DHAVE_CONFIG_H
CFLAGS = -fPIC -O3
CFLAGS += -Wall -Werror

LDFLAGS = -shared -Wl,-soname,$@ -Wl,-z,relro -Wl,-z,now

.PHONY: default
default : b64-builtin ; chmod 644 $<

%-builtin : %-builtin.c; $(CC) $(CPPFLAGS) ${CFLAGS} $< ${LDFLAGS} -o $@

clean:; rm -f b64-builtin
