///\file
///\brief implementation file for psycle::host::PresetIO.

#include <psycle/host/detail/project.private.hpp>
#include "PresetIO.hpp"
#include "Machine.hpp"
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
namespace psycle { namespace host {
		////////////////////////////////////////////////////////////////////////////
		//
		//////////////////////////////////////////////////////////////////////////////

		void PresetIO::LoadPresets(const char* szFile, int numParameters, int dataSizeStruct,  std::list<CPreset>& presets, bool warn_if_not_exists)
		{
			std::FILE* hfile;
			if(hfile=std::fopen(szFile,"rb"))
			{
				int numpresets;
				int filenumpars;
				if ( std::fread(&numpresets,sizeof(int),1,hfile) != 1 ||
					std::fread(&filenumpars,sizeof(int),1,hfile) != 1 )
				{
					::MessageBox(0,"Couldn't read from file. Operation aborted","Preset File Error",MB_OK);
				}
				else if (numpresets >= 0)
				{
					// ok so we still support old file format by checking for a positive numpresets
					if (( filenumpars != numParameters )  || (dataSizeStruct))
					{
						::MessageBox(0,"The current preset File is not up-to-date with the plugin.","Preset File Error",MB_OK);
					}
					else {
						presets.clear();
						LoadVersion0(hfile, numpresets, numParameters, presets);
						presets.sort();
					}
				}
				else
				{
					// new preset file format since numpresets was < 0
					// we will use filenumpars already read as version #
					if (filenumpars == 1)
					{
						presets.clear();
						LoadVersion1(hfile, numParameters, dataSizeStruct, presets);
						presets.sort();
					}
					else
					{
						::MessageBox(0,"The current preset file is from a newer version of psycle than you are currently running.","Preset File Error",MB_OK);
					}
				}
				std::fclose(hfile);
			}
			else if(warn_if_not_exists) {
				::MessageBox(0,"Couldn't open file. Operation aborted","Preset File Error",MB_OK);
			}
		}

		void PresetIO::SavePresets(const char* szFile,  std::list<CPreset>& presets)
		{
			std::FILE* hfile;
			if(presets.size() > 0) {
				if(!(hfile=std::fopen(szFile,"wb")))
				{
					::MessageBox(0,"The File couldn't be opened for Writing. Operation Aborted","File Save Error",MB_OK);
					return;
				}
				if (false)
				{
					SaveVersion0(hfile, presets);
				}
				else if (true)
				{
					SaveVersion1(hfile, presets);
				}
				std::fclose(hfile);
			}
			else {
				boost::filesystem::path thepath(szFile);
				boost::filesystem::remove(thepath);
			}
		}


		void PresetIO::LoadVersion0(FILE* hfile, int numpresets, int numParameters, std::list<CPreset>& presets)
		{			
			char name[32];
			int* ibuf;
			ibuf= new int[numParameters];

			for (int i=0; i< numpresets && !std::feof(hfile) && !std::ferror(hfile); i++ )
			{
				std::fread(name,sizeof(name),1,hfile);
				std::fread(ibuf,numParameters*sizeof(int),1,hfile);
				CPreset preset;
				preset.Init(numParameters,name,ibuf,0,0);
				presets.push_back(preset);
			}
			delete[] ibuf;
		}

		void PresetIO::LoadVersion1(FILE* hfile, int numParameters, int dataSizeStruct, std::list<CPreset>& presets)
		{
			int numpresets;
			int filenumpars;
			int filepresetsize;
			// new preset format version 1
			std::fread(&numpresets,sizeof(int),1,hfile);
			std::fread(&filenumpars,sizeof(int),1,hfile);
			std::fread(&filepresetsize,sizeof(int),1,hfile);
			// now it is time to check our file for compatability
			if (( filenumpars != numParameters )  || (filepresetsize != dataSizeStruct))
			{
				::MessageBox(0,"The current preset File is not up-to-date with the plugin.","Preset File Error",MB_OK);
				return;
			}
			// ok that works, so we should now load the names of all of the presets
			char name[32];
			int* ibuf= new int[numParameters];
			unsigned char * dbuf = 0;
			if ( dataSizeStruct > 0 ) dbuf = new unsigned char[dataSizeStruct];

			for (int i=0; i< numpresets && !feof(hfile) && !ferror(hfile); i++)
			{
				std::fread(name,sizeof(name),1,hfile);
				std::fread(ibuf,numParameters*sizeof(int),1,hfile);
				if ( dataSizeStruct > 0 )  std::fread(dbuf,dataSizeStruct,1,hfile);
				CPreset preset;
				preset.Init(numParameters,name,ibuf,dataSizeStruct,dbuf);
				presets.push_back(preset);
			}
			delete[] ibuf;
			delete[] dbuf;
		}

		void PresetIO::SaveVersion0(FILE* hfile, std::list<CPreset>& presets)
		{
			int numpresets=presets.size();
			std::list<CPreset>::iterator preset = presets.begin();
			int numParameters = preset->GetNumPars();

			if ( std::fwrite(&numpresets,sizeof(int),1,hfile) != 1 ||
				std::fwrite(&numParameters,sizeof(int),1,hfile) != 1 )
			{
				::MessageBox(0,"Couldn't write to File. Operation Aborted","File Save Error",MB_OK);
				return;
			}
			
			char cbuf[32];
			int* ibuf = new int[numParameters];

			for (int i=0; i< numpresets && !feof(hfile) && !ferror(hfile); i++, preset++ )
			{
				preset->GetName(cbuf);
				preset->GetParsArray(ibuf);
				std::fwrite(cbuf,sizeof(cbuf),1,hfile);
				std::fwrite(ibuf,numParameters*sizeof(int),1,hfile);
			}
			delete[] ibuf;
		}

		void PresetIO::SaveVersion1(FILE* hfile, std::list<CPreset>& presets)
		{
			int temp1 = -1;
			int fileversion = 1;
			if ( std::fwrite(&temp1,sizeof(int),1,hfile) != 1 ||
				std::fwrite(&fileversion,sizeof(int),1,hfile) != 1 )
			{
				::MessageBox(0,"Couldn't write to File. Operation Aborted","File Save Error",MB_OK);
				return;
			}
			std::list<CPreset>::iterator preset = presets.begin();
			int numpresets=presets.size();
			int numParameters = preset->GetNumPars();
			int dataSizeStruct = preset->GetDataSize();
			std::fwrite(&numpresets,sizeof(int),1,hfile);
			std::fwrite(&numParameters,sizeof(int),1,hfile);
			std::fwrite(&dataSizeStruct,sizeof(int),1,hfile);
			
			char cbuf[32];
			int* ibuf= new int[numParameters];
			unsigned char * dbuf = 0;
			if ( dataSizeStruct > 0 ) dbuf = new byte[dataSizeStruct];

			for (int i=0; i< numpresets && !std::feof(hfile) && !std::ferror(hfile); i++, preset++)
			{
				preset->GetName(cbuf);
				preset->GetParsArray(ibuf);
				preset->GetDataArray(dbuf);
				std::fwrite(cbuf,sizeof(cbuf),1,hfile);
				std::fwrite(ibuf,numParameters*sizeof(int),1,hfile);
				if ( dataSizeStruct > 0 ) std::fwrite(dbuf,dataSizeStruct,1,hfile);
			}
			delete[] ibuf;
			delete[] dbuf;
		}

		void PresetIO::LoadDefaultMap(std::string szFile, ParamTranslator& translator)
		{
			std::FILE* hfile;
			uint16_t numMaps=0;
			hfile=std::fopen(szFile.c_str(),"rb");
			for (int i = 0; i < 256; ++i) {
				translator.set_virtual_index(i,i);
			}
			if(hfile)
			{
				int fileversion = 1;
				if ( std::fread(&fileversion,sizeof(int),1,hfile) != 1)
				{
					::MessageBox(0,"Couldn't read from file. Operation aborted","Preset File Error",MB_OK);
				}
				else {
					char pmap[4];
					std::fread(pmap,sizeof(char),4, hfile);
					if (fileversion == 1) {
						std::fread(&numMaps,sizeof(uint8_t),1, hfile);
						while (!feof(hfile) && !ferror(hfile))
						{
							uint8_t idx;
							uint16_t value;
							std::fread(&idx,sizeof(idx),1, hfile);
							std::fread(&value,sizeof(value),1, hfile);
							translator.set_virtual_index(idx,value);
						}
					}
				}
				std::fclose(hfile);
			}
		}

		void PresetIO::SaveDefaultMap(std::string szFile, ParamTranslator& translator, int numparams)
		{
			std::FILE* hfile;
			int fileversion = 1;
			uint16_t numMaps=0;
			if(!(hfile=std::fopen(szFile.c_str(),"wb")))
			{
				::MessageBox(0,"The File couldn't be opened for Writing. Operation Aborted","File Save Error",MB_OK);
				return;
			}

			if ( std::fwrite(&fileversion,sizeof(int),1,hfile) != 1 )
			{
				::MessageBox(0,"Couldn't write to File. Operation Aborted","File Save Error",MB_OK);
				return;
			}
			for (int i=0;i<256;i++)
			{
				const int param = translator.translate(i);
				if ( param < numparams) {
					numMaps++;
				}
			}
			std::fwrite("PMAP",sizeof(char),4,hfile);
			std::fwrite(&numMaps,sizeof(numMaps),1,hfile);
			for (int i=0;i<256;i++)
			{
				const int param = translator.translate(i);
				if ( param < numparams) {
					const uint8_t idx = static_cast<uint8_t>(i);
					const uint16_t value = static_cast<uint16_t>(param);
					std::fwrite(&idx,sizeof(idx),1,hfile);
					std::fwrite(&value,sizeof(value),1,hfile);
				}
			}
			std::fclose(hfile);
		}

	}   // namespace
}   // namespace
