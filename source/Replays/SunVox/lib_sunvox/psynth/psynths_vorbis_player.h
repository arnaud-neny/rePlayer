#pragma once

void vplayer_set_filename( int mod_num, char* filename, psynth_net* pnet ); //Vorbis Player can read the stream directly from the specified file (without vplayer_load_file())
uint64_t vplayer_get_pcm_time( int mod_num, psynth_net* pnet ); //PCM offset (frames) of next PCM sample to be read
uint64_t vplayer_get_total_pcm_time( int mod_num, psynth_net* pnet ); //total PCM length (frames)
void vplayer_set_pcm_time( int mod_num, uint64_t t, psynth_net* pnet );
int vplayer_load_file( int mod_num, const char* filename, sfs_file f, psynth_net* pnet );
