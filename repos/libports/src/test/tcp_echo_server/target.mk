TARGET  =  test-tcp_echo_server
LIBS   := stdcxx
LIBS   += base
LIBS   += libc
LIBS   += libc_pipe
SRC_CC += main.cc

CC_CXX_WARN_STRICT =
