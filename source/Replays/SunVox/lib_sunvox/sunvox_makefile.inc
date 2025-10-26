FINAL_CFLAGS += -I$(SUNVOX)

SUNVOX_SRC += \
    sunvox_engine.cpp \
    sunvox_engine_action_handler.cpp \
    sunvox_engine_audio_callback.cpp \
    sunvox_engine_export.cpp \
    sunvox_engine_load_proj.cpp \
    sunvox_engine_load_module.cpp \
    sunvox_engine_patterns.cpp \
    sunvox_engine_record.cpp \
    sunvox_engine_save_proj.cpp \
    sunvox_engine_save_module.cpp \

SUNVOX_SRC2 += \
    midi_file.cpp

SUNVOX_SRC3 += \
    xm_instrum.cpp \
    xm_pattern.cpp \
    xm_sample.cpp \
    xm_song.cpp

SUNVOX_SRC4 += \
    psynth_net.cpp \
    psynth_net_midi_in.cpp \
    psynth_strings.cpp \
    psynth_gui_utils.cpp \
    psynth_dsp.cpp \
    psynths_drumsynth.cpp \
    psynths_fm.cpp \
    psynths_fm2.cpp \
    psynths_generator.cpp \
    psynths_generator2.cpp \
    psynths_input.cpp \
    psynths_kicker.cpp \
    psynths_vorbis_player.cpp \
    psynths_sampler.cpp \
    psynths_spectravoice.cpp \
    psynths_amplifier.cpp \
    psynths_compressor.cpp \
    psynths_dc_blocker.cpp \
    psynths_delay.cpp \
    psynths_distortion.cpp \
    psynths_echo.cpp \
    psynths_eq.cpp \
    psynths_fft.cpp \
    psynths_filter.cpp \
    psynths_filter2.cpp \
    psynths_flanger.cpp \
    psynths_lfo.cpp \
    psynths_loop.cpp \
    psynths_modulator.cpp \
    psynths_pitch_shifter.cpp \
    psynths_reverb.cpp \
    psynths_smooth.cpp \
    psynths_vibrato.cpp \
    psynths_vocal_filter.cpp \
    psynths_waveshaper.cpp \
    psynths_adsr.cpp \
    psynths_ctl2note.cpp \
    psynths_feedback.cpp \
    psynths_glide.cpp \
    psynths_gpio.cpp \
    psynths_metamodule.cpp \
    psynths_multictl.cpp \
    psynths_multisynth.cpp \
    psynths_pitch2ctl.cpp \
    psynths_pitch_detector.cpp \
    psynths_sound2ctl.cpp \
    psynths_velocity2ctl.cpp

SUNVOX_OBJS = $(SUNVOX_SRC:.cpp=.o)
SUNVOX_OBJS2 = $(SUNVOX_SRC2:.cpp=.o)
SUNVOX_OBJS3 = $(SUNVOX_SRC3:.cpp=.o)
SUNVOX_OBJS4 = $(SUNVOX_SRC4:.cpp=.o)
OBJS += $(SUNVOX_OBJS) $(SUNVOX_OBJS2) $(SUNVOX_OBJS3) $(SUNVOX_OBJS4)

SUNVOX_DEPS = $(SUNVOX)*.h $(SUNVOX)psynth/*.h $(SUNVOX)midi_file/*.h $(SUNVOX)xm/*.h 

$(SUNVOX_OBJS): %.o: $(SUNVOX)%.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<

$(SUNVOX_OBJS2): %.o: $(SUNVOX)midi_file/%.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<

$(SUNVOX_OBJS3): %.o: $(SUNVOX)xm/%.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<

$(SUNVOX_OBJS4): %.o: $(SUNVOX)psynth/%.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
