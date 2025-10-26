TREMOR_ORIG = $(VORBIS)tremor/
VORBIS_ORIG = $(VORBIS)libvorbis/
OGG_ORIG = $(VORBIS)libogg/

FINAL_CFLAGS += -I$(TREMOR_ORIG)
FINAL_CFLAGS += -I$(VORBIS_ORIG)
FINAL_CFLAGS += -I$(OGG_ORIG)

TREMOR_DEPS = $(TREMOR_ORIG)tremor/*.h
VORBIS_DEPS = $(VORBIS_ORIG)*.h $(VORBIS_ORIG)vorbis/*.h
OGG_DEPS = $(OGG_ORIG)*.h $(OGG_ORIG)ogg/*.h



TREMOR_SRC += \
    bitwise.c \
    codebook.c \
    dsp.c \
    floor0.c \
    floor1.c \
    floor_lookup.c \
    framing.c \
    info.c \
    mapping0.c \
    mdct.c \
    misc.c \
    res012.c \
    vorbisfile.c

TREMOR_OBJS = $(TREMOR_SRC:%.c=tremor_%.o)
OBJS += $(TREMOR_OBJS)



ifeq ($(MAKE_WITHOUT_LIBVORBIS_ENCODER),true)
    FINAL_CFLAGS += -DNOOGGENC
else
VORBIS_SRC += \
    vorbisenc.c
endif

MAKE_WITHOUT_LIBVORBIS_DECODER = true
ifeq ($(MAKE_WITHOUT_LIBVORBIS_DECODER),true)
else
VORBIS_SRC += \
    lookup.c \
    synthesis.c \
    vorbisfile.c
endif

ifeq ($(MAKE_WITHOUT_LIBVORBIS_ENCODER),true)
ifeq ($(MAKE_WITHOUT_LIBVORBIS_DECODER),true)
MAKE_WITHOUT_LIBVORBIS = true
endif
endif

ifneq ($(MAKE_WITHOUT_LIBVORBIS),true)
VORBIS_SRC += \
    analysis.c \
    bitrate.c \
    block.c \
    codebook.c \
    envelope.c \
    floor0.c \
    floor1.c \
    info.c \
    lpc.c \
    lsp.c \
    mapping0.c \
    mdct.c \
    psy.c \
    registry.c \
    res0.c \
    sharedbook.c \
    smallft.c \
    window.c
endif

VORBIS_OBJS = $(VORBIS_SRC:%.c=vorbis_%.o)
OBJS += $(VORBIS_OBJS)



OGG_SRC += \
    bitwise.c \
    framing.c

OGG_OBJS = $(OGG_SRC:%.c=ogg_%.o)
OBJS += $(OGG_OBJS)



$(TREMOR_OBJS): tremor_%.o: $(TREMOR_ORIG)%.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $< -o $@

$(VORBIS_OBJS): vorbis_%.o: $(VORBIS_ORIG)%.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $< -o $@

$(OGG_OBJS): ogg_%.o: $(OGG_ORIG)%.c $(OGG_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $< -o $@
