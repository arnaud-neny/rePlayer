FLAC_ORIG = $(FLAC)libFLAC/

FINAL_CFLAGS += -I$(FLAC_ORIG)

ifeq ($(MAKE_WITHOUT_FLAC_ENCODER),true)
    FINAL_CFLAGS += -DNOFLACENC
else
OBJS += \
    flac_bitwriter.o \
    flac_float.o \
    flac_metadata_iterators.o \
    flac_metadata_object.o \
    flac_stream_encoder.o \
    flac_stream_encoder_intrin_sse2.o \
    flac_stream_encoder_intrin_ssse3.o \
    flac_stream_encoder_framing.o \
    flac_window.o
endif

OBJS += \
    flac_bitmath.o \
    flac_bitreader.o \
    flac_cpu.o \
    flac_crc.o \
    flac_fixed.o \
    flac_fixed_intrin_sse2.o \
    flac_fixed_intrin_ssse3.o \
    flac_fixed_intrin_sse42.o \
    flac_format.o \
    flac_lpc.o \
    flac_lpc_intrin_sse2.o \
    flac_lpc_intrin_sse41.o \
    flac_lpc_intrin_neon.o \
    flac_md5.o \
    flac_memory.o \
    flac_stream_decoder.o \
    flac_win_utf8_io.o

FLAC_DEPS = $(FLAC_ORIG)*.h $(FLAC_ORIG)FLAC/*.h

flac_bitmath.o: $(FLAC_ORIG)bitmath.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)bitmath.c -o flac_bitmath.o

flac_bitreader.o: $(FLAC_ORIG)bitreader.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)bitreader.c -o flac_bitreader.o

flac_bitwriter.o: $(FLAC_ORIG)bitwriter.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)bitwriter.c -o flac_bitwriter.o

flac_cpu.o: $(FLAC_ORIG)cpu.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)cpu.c -o flac_cpu.o

flac_crc.o: $(FLAC_ORIG)crc.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)crc.c -o flac_crc.o

flac_fixed.o: $(FLAC_ORIG)fixed.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)fixed.c -o flac_fixed.o

flac_fixed_intrin_sse2.o: $(FLAC_ORIG)fixed_intrin_sse2.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)fixed_intrin_sse2.c -o flac_fixed_intrin_sse2.o

flac_fixed_intrin_sse42.o: $(FLAC_ORIG)fixed_intrin_sse42.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)fixed_intrin_sse42.c -o flac_fixed_intrin_sse42.o

flac_fixed_intrin_ssse3.o: $(FLAC_ORIG)fixed_intrin_ssse3.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)fixed_intrin_ssse3.c -o flac_fixed_intrin_ssse3.o

flac_float.o: $(FLAC_ORIG)float.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)float.c -o flac_float.o

flac_format.o: $(FLAC_ORIG)format.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)format.c -o flac_format.o

flac_lpc.o: $(FLAC_ORIG)lpc.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)lpc.c -o flac_lpc.o

flac_lpc_intrin_neon.o: $(FLAC_ORIG)lpc_intrin_neon.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)lpc_intrin_neon.c -o flac_lpc_intrin_neon.o

flac_lpc_intrin_sse2.o: $(FLAC_ORIG)lpc_intrin_sse2.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)lpc_intrin_sse2.c -o flac_lpc_intrin_sse2.o

flac_lpc_intrin_sse41.o: $(FLAC_ORIG)lpc_intrin_sse41.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)lpc_intrin_sse41.c -o flac_lpc_intrin_sse41.o

flac_md5.o: $(FLAC_ORIG)md5.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)md5.c -o flac_md5.o

flac_memory.o: $(FLAC_ORIG)memory.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)memory.c -o flac_memory.o

flac_metadata_iterators.o: $(FLAC_ORIG)metadata_iterators.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)metadata_iterators.c -o flac_metadata_iterators.o

flac_metadata_object.o: $(FLAC_ORIG)metadata_object.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)metadata_object.c -o flac_metadata_object.o

flac_stream_decoder.o: $(FLAC_ORIG)stream_decoder.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)stream_decoder.c -o flac_stream_decoder.o

flac_stream_encoder.o: $(FLAC_ORIG)stream_encoder.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)stream_encoder.c -o flac_stream_encoder.o

flac_stream_encoder_framing.o: $(FLAC_ORIG)stream_encoder_framing.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)stream_encoder_framing.c -o flac_stream_encoder_framing.o

flac_stream_encoder_intrin_sse2.o: $(FLAC_ORIG)stream_encoder_intrin_sse2.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)stream_encoder_intrin_sse2.c -o flac_stream_encoder_intrin_sse2.o

flac_stream_encoder_intrin_ssse3.o: $(FLAC_ORIG)stream_encoder_intrin_ssse3.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)stream_encoder_intrin_ssse3.c -o flac_stream_encoder_intrin_ssse3.o

flac_window.o: $(FLAC_ORIG)window.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)window.c -o flac_window.o

flac_win_utf8_io.o: $(FLAC_ORIG)win_utf8_io.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) $(FLAC_DEFINES) -c $(FLAC_ORIG)win_utf8_io.c -o flac_win_utf8_io.o
