# Makefile, 2012-06-15 T1738
 
# TODO
# ----------------------------------
NAME        = intraportmap
VERSION     = 1.0.0
RELEASE     = 01
# ----------------------------------
 
BIN         = $(NAME)
OBJS        = $(patsubst %.cpp,%.o,$(wildcard $(SRC_DIR)/*.cpp))
 
# TODO
# BEGIN ----------------------------------
# Folders
SRC_DIR     = .
 
# Flags
CXXFLAGS    = -Wall -std=c++0x
CXXFLAGS   += -I$(SRC_DIR)
 
#CPPFLAGS    = -D_DEBUG
CPPFLAGS    = -O2
 
LDFLAGS     = -L$(SRC_DIR) -static -lgcc_pic -levent
 
LIBS        = 
# END ----------------------------------
 
.PHONY: all clean
 
all: $(BIN)
 
$(BIN): $(OBJS)
	$(CXX) $(CPPFLAGS) $^ -o $@ $(LIBS) $(LDFLAGS)
 
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $^ -o $@
 
clean:
	$(RM) $(OBJS) $(BIN)
 

