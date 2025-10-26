FLAC_ORIG = $(FLAC)libFLAC/

FINAL_CFLAGS += -I$(FLAC_ORIG)

ifeq ($(MAKE_WITHOUT_FLAC_ENCODER),true)
    FINAL_CFLAGS += -DNOFLACENC
else
FLAC_SRC += \
    bitwriter.c \
    float.c \
    metadata_iterators.c \
    metadata_object.c \
    stream_encoder.c \
    stream_encoder_intrin_sse2.c \
    stream_encoder_intrin_ssse3.c \
    stream_encoder_framing.c \
    window.c
endif

FLAC_SRC += \
    bitmath.c \
    bitreader.c \
    cpu.c \
    crc.c \
    fixed.c \
    fixed_intrin_sse2.c \
    fixed_intrin_ssse3.c \
    fixed_intrin_sse42.c \
    format.c \
    lpc.c \
    lpc_intrin_sse2.c \
    lpc_intrin_sse41.c \
    lpc_intrin_neon.c \
    md5.c \
    memory.c \
    stream_decoder.c \
    win_utf8_io.c

FLAC_OBJS = $(FLAC_SRC:%.c=flac_%.o)
OBJS += $(FLAC_OBJS)

FLAC_DEPS = $(FLAC_ORIG)*.h $(FLAC_ORIG)FLAC/*.h

$(FLAC_OBJS): flac_%.o: $(FLAC_ORIG)%.c $(FLAC_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $< -o $@
