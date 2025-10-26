FINAL_CFLAGS += -I$(DSP)

DSP_SRC = \
    dsp_tables.cpp \
    dsp_functions.cpp

DSP_OBJS = $(DSP_SRC:.cpp=.o)
OBJS += $(DSP_OBJS)

DSP_DEPS = $(DSP)*.h

$(DSP_OBJS): %.o: $(DSP)%.cpp $(DSP_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
