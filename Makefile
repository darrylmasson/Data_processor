CC = g++
ROOT = $(shell root-config --cflags --libs)
OBJDIR = obj
CFLAGS = -g -Wall -Iinc -O
OUTDIR = /data/NeutronGenerator
INSTALL = -o $(OUTDIR)/NG_dp
TEST = -o test_exe
SRCS = Digitizer.cpp \
			 Event.cpp \
			 CCM.cpp \
			 DFT.cpp \
			 XSQ.cpp \
			 LAP.cpp \
			 Config.cpp \
			 Processor.cpp \
			 NG_dp.cpp
OBJS = $(SRCS:.cpp=.o)
VPATH = src

test : $(L)$(OBJS)
	$(CC) $(CFLAGS) $(TEST) $(addprefix $(OBJDIR)/,$(OBJS)) $(ROOT)

install : $(L)$(OBJS)
	$(CC) $(CFLAGS) $(INSTALL) $(addprefix $(OBJDIR)/,$(OBJS)) $(ROOT)

$(L)%.o : %.cpp
	$(CC) $(CFLAGS) -c $< -o $(OBJDIR)/$@ $(ROOT)

.PHONY: clean

clean:
	rm -f $(OBJDIR)/*.o
