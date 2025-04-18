FINAL_CFLAGS += -I$(DSP)

OBJS += \
    dsp_tables.o \
    dsp_functions.o

dsp_tables.o: $(DSP)dsp_tables.cpp $(DSP)dsp.h $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(DSP)dsp_tables.cpp

dsp_functions.o: $(DSP)dsp_functions.cpp $(DSP)dsp.h $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(DSP)dsp_functions.cpp
