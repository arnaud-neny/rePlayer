#pragma once

typedef int32_t SMPPTR; //must be signed; byte number OR frame number; max sample size = 2GB;
//Before moving to 64 bits, you must also change int32 to int64 in the sample and instrument structs (and add some chunk converter)
// --> but this will break compatibility with older versions -->
// --> so, it will be better to add additional int64 variables to these structures?

int sampler_load( const char* filename, sfs_file f, int mod_num, psynth_net* net, int sample_num, bool load_unsupported_files_as_raw );

int sampler_par( psynth_net* net, int mod_num, int smp_num, int par_num, int par_val, int set );
/*
Set/get sample parameter:
  0 - Loop begin: 0 ... (sample_length - 1);
  1 - Loop length: 0 ... (sample_length - loop_begin);
  2 - Loop type: 0 - none; 1 - fwd; 2 - bidirectional;
  3 - Loop release flag: 0 - none; 1 - loop will be finished after the note release;
  4 - Volume: 0 ... 64;
  5 - Panning: 0 (left) ... 128 (center) ... 255 (right);
  6 - Finetune: -128 ... 0 ... +127 (higher value = higher pitch);
  7 - Relative note: -128 ... 0 ... +127 (higher value = higher pitch);
  8 - Start position: 0 ... (sample_length - 1);
*/
