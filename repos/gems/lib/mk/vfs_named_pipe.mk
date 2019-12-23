SRC_CC = file_system.cc pipe_handle.cc plugin.cc pipe.cc

vpath %.cc $(REP_DIR)/src/lib/vfs/named_pipe

SHARED_LIB = yes
