///\file
///\brief implementation file for psycle::host::KeyPresetIO.

#include <psycle/host/detail/project.private.hpp>
#include "KeyPresetIO.hpp"

namespace psycle { namespace host {

		const char * sDefaultCfgName = "PsycleKeys.INI";

		void KeyPresetIO::LoadPreset(const char* szFile, PsycleConfig::InputHandler& settings)
		{
			std::FILE * hfile;
			char buf[1 << 10];
			if(!(hfile=std::fopen(szFile,"r")))
			{
				::MessageBox(0, "Couldn't open File for Reading. Operation Aborted",
					"File Open Error", MB_ICONERROR | MB_OK);
				return;
			}

			while(std::fgets(buf, sizeof buf, hfile))
			{
				if(buf[0] == '#' || buf[0] == ';')
				{ 
					// Skip comments
					continue;
				} 
				if(buf[0] == '[')
				{
					const char* test = "[Psycle Keymap Presets v1.0]\n";
					if(strcmp(buf,test))
					{
						::MessageBox(0, "This loader only supports Psycle Keymap format 1.0",
							"File Open Error", MB_ICONERROR | MB_OK);
						return;
					}
					continue;
				}
				//locate = sign
				int length = std::strlen(buf);
				int pos = std::strspn(buf,"=");
				if (pos < length)
				{
					//Separete in two parts on the equal sign
					char* value = &buf[pos+1];
					buf[pos]='\0';

					//Get the mod and key out of the first part
					pos = std::strspn(buf,"Key[");
					//minus 1 because of the \n
					length = std::strlen(buf)-1;
					if(pos + 6 < length)
					{
						int mod = buf[pos+4] - '0';
						int key = atoi(&buf[pos+6]);
						value[5] = '\0';
						CmdSet ID = CmdSet(atoi(value));
						settings.SetCmd(ID, mod, key);
					}
				}
			}
			
			std::fclose(hfile);
		}

		void KeyPresetIO::SavePreset(const char* szFile, PsycleConfig::InputHandler& settings)
		{
			std::FILE * hfile;
			::CString str = szFile;
			::CString str2 = str.Right(4);
			if ( str2.CompareNoCase(".psk") != 0 ) str.Insert(str.GetLength(),".psk");
			if(!(hfile=std::fopen(str,"wb")))
			{
				::MessageBox(0, "Couldn't open File for Writing. Operation Aborted",
					"File Save Error", MB_ICONERROR | MB_OK);
				return;
			}
			std::fprintf(hfile,"[Psycle Keymap Presets v1.0]\n\n");

			std::map<std::pair<int,int>,CmdDef>::const_iterator it;
			for(it = settings.keyMap.begin(); it != settings.keyMap.end(); ++it)
			{
				if (it->second.IsValid()) {
					std::fprintf(hfile,"Key[%d]%03d=%03d     ; cmd = '%s'\n",
						it->first.first,
						it->first.second,
						it->second.GetID(),
						it->second.GetName());
				}
			}

			std::fclose(hfile);
		}

		void KeyPresetIO::LoadOldPrivateProfile(PsycleConfig::InputHandler& settings)
		{
			CString sect;
			CString key;
			CString data;

			// check file
			sect = "Info";
			key = "AppVersion";
			data = "";	
			GetPrivateProfileString(sect,key,"",data.GetBufferSetLength(64),64,sDefaultCfgName);
			if(data=="")
			{
				return;
			}
			// option data
			sect = "Options";
			key = "bNewHomeBehaviour";
			settings.bFT2HomeBehaviour = GetPrivateProfileInt(sect,key,1,sDefaultCfgName)?true:false;

			key = "bCtrlPlay";
			settings.bCtrlPlay = GetPrivateProfileInt(sect,key,1,sDefaultCfgName)?true:false;

			key = "bMoveCursorPaste";
			settings.bMoveCursorPaste = GetPrivateProfileInt(sect,key,1,sDefaultCfgName)?true:false;

			key = "bFT2DelBehaviour";
			settings.bFT2DelBehaviour = GetPrivateProfileInt(sect,key,1,sDefaultCfgName)?true:false;

			key = "bShiftArrowsDoSelect";
			settings.bShiftArrowsDoSelect = GetPrivateProfileInt(sect,key,1,sDefaultCfgName)?true:false;
			
			if (data != "N/A")  // 1.7.6 and older had N/A in this field. No longer supported.
			{
				CString cmdDefn;
				int cmddata;
				// restore key data
				sect = "Keys2"; // 1.8 onward
				settings.keyMap.clear();
				std::map<CmdSet,std::pair<int,int>>::const_iterator it;
				for(it = settings.setMap.begin(); it != settings.setMap.end(); ++it)
				{
					CmdDef cmd(it->first);
					if(cmd.IsValid())
					{
						key.Format("n%03d",it->first);
						cmddata= GetPrivateProfileInt(sect,key,0,sDefaultCfgName);
						settings.SetCmd(cmd.GetID(),cmddata>>8,cmddata&0xFF);
					}
				}
			}
		}
		

	}   // namespace
}   // namespace
