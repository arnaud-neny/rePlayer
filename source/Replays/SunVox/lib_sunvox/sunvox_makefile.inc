FINAL_CFLAGS += -I$(SUNVOX)

OBJS += \
    sunvox_engine.o \
    sunvox_engine_action_handler.o \
    sunvox_engine_audio_callback.o \
    sunvox_engine_export.o \
    sunvox_engine_load_proj.o \
    sunvox_engine_load_module.o \
    sunvox_engine_patterns.o \
    sunvox_engine_record.o \
    sunvox_engine_save_proj.o \
    sunvox_engine_save_module.o \
    midi_file.o \
    xm_instrum.o \
    xm_pattern.o \
    xm_sample.o \
    xm_song.o \
    psynth_net.o \
    psynth_net_midi_in.o \
    psynth_strings.o \
    psynth_gui_utils.o \
    psynth_dsp.o \
    psynths_drumsynth.o \
    psynths_fm.o \
    psynths_fm2.o \
    psynths_generator.o \
    psynths_generator2.o \
    psynths_input.o \
    psynths_kicker.o \
    psynths_vorbis_player.o \
    psynths_sampler.o \
    psynths_spectravoice.o \
    psynths_amplifier.o \
    psynths_compressor.o \
    psynths_dc_blocker.o \
    psynths_delay.o \
    psynths_distortion.o \
    psynths_echo.o \
    psynths_eq.o \
    psynths_fft.o \
    psynths_filter.o \
    psynths_filter2.o \
    psynths_flanger.o \
    psynths_lfo.o \
    psynths_loop.o \
    psynths_modulator.o \
    psynths_pitch_shifter.o \
    psynths_reverb.o \
    psynths_smooth.o \
    psynths_vibrato.o \
    psynths_vocal_filter.o \
    psynths_waveshaper.o \
    psynths_adsr.o \
    psynths_ctl2note.o \
    psynths_feedback.o \
    psynths_glide.o \
    psynths_gpio.o \
    psynths_metamodule.o \
    psynths_multictl.o \
    psynths_multisynth.o \
    psynths_pitch2ctl.o \
    psynths_pitch_detector.o \
    psynths_sound2ctl.o \
    psynths_velocity2ctl.o

SUNVOX_DEPS = \
    $(SUNVOX)sunvox_engine.h \
    $(SUNVOX)xm/xm.h \
    $(SUNVOX)psynth/psynth.h \
    $(SUNVOX)psynth/psynth_net.h \
    $(SUNVOX)psynth/psynth_dsp.h \
    $(SUNVOX)psynth/psynth_gui_utils.h \
    $(SUNVOX)psynth/psynth_strings.h

sunvox_engine.o: $(SUNVOX)sunvox_engine.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine.cpp

sunvox_engine_action_handler.o: $(SUNVOX)sunvox_engine_action_handler.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_action_handler.cpp

sunvox_engine_audio_callback.o: $(SUNVOX)sunvox_engine_audio_callback.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_audio_callback.cpp

sunvox_engine_export.o: $(SUNVOX)sunvox_engine_export.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_export.cpp

sunvox_engine_load_proj.o: $(SUNVOX)sunvox_engine_load_proj.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_load_proj.cpp

sunvox_engine_load_module.o: $(SUNVOX)sunvox_engine_load_module.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_load_module.cpp

sunvox_engine_patterns.o: $(SUNVOX)sunvox_engine_patterns.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_patterns.cpp

sunvox_engine_record.o: $(SUNVOX)sunvox_engine_record.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_record.cpp

sunvox_engine_save_proj.o: $(SUNVOX)sunvox_engine_save_proj.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_save_proj.cpp

sunvox_engine_save_module.o: $(SUNVOX)sunvox_engine_save_module.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)sunvox_engine_save_module.cpp

midi_file.o: $(SUNVOX)midi_file/midi_file.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)midi_file/midi_file.cpp

xm_instrum.o: $(SUNVOX)xm/xm_instrum.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)xm/xm_instrum.cpp

xm_pattern.o: $(SUNVOX)xm/xm_pattern.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)xm/xm_pattern.cpp

xm_sample.o: $(SUNVOX)xm/xm_sample.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)xm/xm_sample.cpp

xm_song.o: $(SUNVOX)xm/xm_song.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)xm/xm_song.cpp

psynth_net.o: $(SUNVOX)psynth/psynth_net.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynth_net.cpp

psynth_strings.o: $(SUNVOX)psynth/psynth_strings.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynth_strings.cpp

psynth_net_midi_in.o: $(SUNVOX)psynth/psynth_net_midi_in.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynth_net_midi_in.cpp

psynth_gui_utils.o: $(SUNVOX)psynth/psynth_gui_utils.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynth_gui_utils.cpp

psynth_dsp.o: $(SUNVOX)psynth/psynth_dsp.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynth_dsp.cpp

psynths_drumsynth.o: $(SUNVOX)psynth/psynths_drumsynth.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_drumsynth.cpp

psynths_fm.o: $(SUNVOX)psynth/psynths_fm.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_fm.cpp

psynths_fm2.o: $(SUNVOX)psynth/psynths_fm2.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_fm2.cpp

psynths_generator.o: $(SUNVOX)psynth/psynths_generator.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_generator.cpp

psynths_generator2.o: $(SUNVOX)psynth/psynths_generator2.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_generator2.cpp

psynths_input.o: $(SUNVOX)psynth/psynths_input.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_input.cpp

psynths_kicker.o: $(SUNVOX)psynth/psynths_kicker.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_kicker.cpp

psynths_sampler.o: $(SUNVOX)psynth/psynths_sampler.cpp $(SUNVOX)psynth/psynths_sampler_gui.h $(SUNVOX)psynth/psynths_sampler_gui_env.h $(SUNVOX)psynth/psynths_sampler_gui_smp.h $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_sampler.cpp

psynths_spectravoice.o: $(SUNVOX)psynth/psynths_spectravoice.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_spectravoice.cpp

psynths_vorbis_player.o: $(SUNVOX)psynth/psynths_vorbis_player.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_vorbis_player.cpp

psynths_amplifier.o: $(SUNVOX)psynth/psynths_amplifier.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_amplifier.cpp

psynths_compressor.o: $(SUNVOX)psynth/psynths_compressor.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_compressor.cpp

psynths_dc_blocker.o: $(SUNVOX)psynth/psynths_dc_blocker.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_dc_blocker.cpp

psynths_delay.o: $(SUNVOX)psynth/psynths_delay.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_delay.cpp

psynths_distortion.o: $(SUNVOX)psynth/psynths_distortion.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_distortion.cpp

psynths_echo.o: $(SUNVOX)psynth/psynths_echo.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_echo.cpp

psynths_eq.o: $(SUNVOX)psynth/psynths_eq.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_eq.cpp

psynths_fft.o: $(SUNVOX)psynth/psynths_fft.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_fft.cpp

psynths_filter.o: $(SUNVOX)psynth/psynths_filter.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_filter.cpp

psynths_filter2.o: $(SUNVOX)psynth/psynths_filter2.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_filter2.cpp

psynths_flanger.o: $(SUNVOX)psynth/psynths_flanger.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_flanger.cpp

psynths_lfo.o: $(SUNVOX)psynth/psynths_lfo.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_lfo.cpp

psynths_loop.o: $(SUNVOX)psynth/psynths_loop.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_loop.cpp

psynths_modulator.o: $(SUNVOX)psynth/psynths_modulator.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_modulator.cpp

psynths_pitch_shifter.o: $(SUNVOX)psynth/psynths_pitch_shifter.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_pitch_shifter.cpp

psynths_reverb.o: $(SUNVOX)psynth/psynths_reverb.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_reverb.cpp

psynths_smooth.o: $(SUNVOX)psynth/psynths_smooth.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_smooth.cpp

psynths_vibrato.o: $(SUNVOX)psynth/psynths_vibrato.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_vibrato.cpp

psynths_vocal_filter.o: $(SUNVOX)psynth/psynths_vocal_filter.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_vocal_filter.cpp

psynths_waveshaper.o: $(SUNVOX)psynth/psynths_waveshaper.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_waveshaper.cpp

psynths_adsr.o: $(SUNVOX)psynth/psynths_adsr.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_adsr.cpp

psynths_ctl2note.o: $(SUNVOX)psynth/psynths_ctl2note.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_ctl2note.cpp

psynths_feedback.o: $(SUNVOX)psynth/psynths_feedback.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_feedback.cpp

psynths_glide.o: $(SUNVOX)psynth/psynths_glide.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_glide.cpp

psynths_gpio.o: $(SUNVOX)psynth/psynths_gpio.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_gpio.cpp

psynths_metamodule.o: $(SUNVOX)psynth/psynths_metamodule.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_metamodule.cpp

psynths_multictl.o: $(SUNVOX)psynth/psynths_multictl.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_multictl.cpp

psynths_multisynth.o: $(SUNVOX)psynth/psynths_multisynth.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_multisynth.cpp

psynths_pitch2ctl.o: $(SUNVOX)psynth/psynths_pitch2ctl.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_pitch2ctl.cpp

psynths_pitch_detector.o: $(SUNVOX)psynth/psynths_pitch_detector.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_pitch_detector.cpp

psynths_sound2ctl.o: $(SUNVOX)psynth/psynths_sound2ctl.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_sound2ctl.cpp

psynths_velocity2ctl.o: $(SUNVOX)psynth/psynths_velocity2ctl.cpp $(SUNVOX_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $(SUNVOX)psynth/psynths_velocity2ctl.cpp
