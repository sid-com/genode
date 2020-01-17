TARGET = test-libc_named_pipe_compare

LIBS := base
LIBS += libc
LIBS += stdcxx
LIBS += vfs

SRC_CC := main.cc

CC_CXX_WARN_STRICT =
