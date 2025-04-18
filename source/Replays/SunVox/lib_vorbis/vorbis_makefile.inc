TREMOR_ORIG = $(VORBIS)tremor/
VORBIS_ORIG = $(VORBIS)libvorbis/
OGG_ORIG = $(VORBIS)libogg/

FINAL_CFLAGS += -I$(TREMOR_ORIG)
FINAL_CFLAGS += -I$(VORBIS_ORIG)
FINAL_CFLAGS += -I$(OGG_ORIG)

TREMOR_DEPS = $(TREMOR_ORIG)tremor/*.h
VORBIS_DEPS = $(VORBIS_ORIG)*.h $(VORBIS_ORIG)vorbis/*.h
OGG_DEPS = $(OGG_ORIG)*.h $(OGG_ORIG)ogg/*.h

OBJS += \
    tremor_bitwise.o \
    tremor_codebook.o \
    tremor_dsp.o \
    tremor_floor0.o \
    tremor_floor1.o \
    tremor_floor_lookup.o \
    tremor_framing.o \
    tremor_info.o \
    tremor_mapping0.o \
    tremor_mdct.o \
    tremor_misc.o \
    tremor_res012.o \
    tremor_vorbisfile.o \

tremor_bitwise.o: $(TREMOR_ORIG)bitwise.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)bitwise.c -o tremor_bitwise.o

tremor_codebook.o: $(TREMOR_ORIG)codebook.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)codebook.c -o tremor_codebook.o

tremor_dsp.o: $(TREMOR_ORIG)dsp.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)dsp.c -o tremor_dsp.o

tremor_floor0.o: $(TREMOR_ORIG)floor0.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)floor0.c -o tremor_floor0.o

tremor_floor1.o: $(TREMOR_ORIG)floor1.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)floor1.c -o tremor_floor1.o

tremor_floor_lookup.o: $(TREMOR_ORIG)floor_lookup.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)floor_lookup.c -o tremor_floor_lookup.o

tremor_framing.o: $(TREMOR_ORIG)framing.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)framing.c -o tremor_framing.o

tremor_info.o: $(TREMOR_ORIG)info.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)info.c -o tremor_info.o

tremor_mapping0.o: $(TREMOR_ORIG)mapping0.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)mapping0.c -o tremor_mapping0.o

tremor_mdct.o: $(TREMOR_ORIG)mdct.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)mdct.c -o tremor_mdct.o

tremor_misc.o: $(TREMOR_ORIG)misc.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)misc.c -o tremor_misc.o

tremor_res012.o: $(TREMOR_ORIG)res012.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)res012.c -o tremor_res012.o

tremor_vorbisfile.o: $(TREMOR_ORIG)vorbisfile.c $(TREMOR_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(TREMOR_ORIG)vorbisfile.c -o tremor_vorbisfile.o

ifeq ($(MAKE_WITHOUT_LIBVORBIS_ENCODER),true)
    FINAL_CFLAGS += -DNOOGGENC
else
OBJS += \
    vorbis_vorbisenc.o
# + some other unused files? You can find them when libvorbis decoder will be used
endif

MAKE_WITHOUT_LIBVORBIS_DECODER = true
ifeq ($(MAKE_WITHOUT_LIBVORBIS_DECODER),true)
else
OBJS += \
    vorbis_lookup.o \
    vorbis_synthesis.o \
    vorbis_vorbisfile.o
endif

ifeq ($(MAKE_WITHOUT_LIBVORBIS_ENCODER),true)
ifeq ($(MAKE_WITHOUT_LIBVORBIS_DECODER),true)
MAKE_WITHOUT_LIBVORBIS = true
endif
endif

ifneq ($(MAKE_WITHOUT_LIBVORBIS),true)
OBJS += \
    vorbis_analysis.o \
    vorbis_bitrate.o \
    vorbis_block.o \
    vorbis_codebook.o \
    vorbis_envelope.o \
    vorbis_floor0.o \
    vorbis_floor1.o \
    vorbis_info.o \
    vorbis_lpc.o \
    vorbis_lsp.o \
    vorbis_mapping0.o \
    vorbis_mdct.o \
    vorbis_psy.o \
    vorbis_registry.o \
    vorbis_res0.o \
    vorbis_sharedbook.o \
    vorbis_smallft.o \
    vorbis_window.o
endif

vorbis_analysis.o: $(VORBIS_ORIG)analysis.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)analysis.c -o vorbis_analysis.o

vorbis_bitrate.o: $(VORBIS_ORIG)bitrate.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)bitrate.c -o vorbis_bitrate.o

vorbis_block.o: $(VORBIS_ORIG)block.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)block.c -o vorbis_block.o

vorbis_codebook.o: $(VORBIS_ORIG)codebook.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)codebook.c -o vorbis_codebook.o

vorbis_envelope.o: $(VORBIS_ORIG)envelope.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)envelope.c -o vorbis_envelope.o

vorbis_floor0.o: $(VORBIS_ORIG)floor0.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)floor0.c -o vorbis_floor0.o

vorbis_floor1.o: $(VORBIS_ORIG)floor1.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)floor1.c -o vorbis_floor1.o

vorbis_info.o: $(VORBIS_ORIG)info.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)info.c -o vorbis_info.o

vorbis_lookup.o: $(VORBIS_ORIG)lookup.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)lookup.c -o vorbis_lookup.o

vorbis_lpc.o: $(VORBIS_ORIG)lpc.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)lpc.c -o vorbis_lpc.o

vorbis_lsp.o: $(VORBIS_ORIG)lsp.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)lsp.c -o vorbis_lsp.o

vorbis_mapping0.o: $(VORBIS_ORIG)mapping0.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)mapping0.c -o vorbis_mapping0.o

vorbis_mdct.o: $(VORBIS_ORIG)mdct.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)mdct.c -o vorbis_mdct.o

vorbis_psy.o: $(VORBIS_ORIG)psy.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)psy.c -o vorbis_psy.o

vorbis_registry.o: $(VORBIS_ORIG)registry.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)registry.c -o vorbis_registry.o

vorbis_res0.o: $(VORBIS_ORIG)res0.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)res0.c -o vorbis_res0.o

vorbis_sharedbook.o: $(VORBIS_ORIG)sharedbook.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)sharedbook.c -o vorbis_sharedbook.o

vorbis_smallft.o: $(VORBIS_ORIG)smallft.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)smallft.c -o vorbis_smallft.o

vorbis_synthesis.o: $(VORBIS_ORIG)synthesis.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)synthesis.c -o vorbis_synthesis.o

vorbis_vorbisenc.o: $(VORBIS_ORIG)vorbisenc.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)vorbisenc.c -o vorbis_vorbisenc.o

vorbis_vorbisfile.o: $(VORBIS_ORIG)vorbisfile.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)vorbisfile.c -o vorbis_vorbisfile.o

vorbis_window.o: $(VORBIS_ORIG)window.c $(VORBIS_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)window.c -o vorbis_window.o

OBJS += \
    ogg_bitwise.o \
    ogg_framing.o

ogg_bitwise.o: $(OGG_ORIG)bitwise.c $(OGG_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(OGG_ORIG)bitwise.c -o ogg_bitwise.o

ogg_framing.o: $(OGG_ORIG)framing.c $(OGG_DEPS)
	$(CC) $(FINAL_CFLAGS) -c $(OGG_ORIG)framing.c -o ogg_framing.o
