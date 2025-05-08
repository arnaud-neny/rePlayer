/*****************************************************************************/
/* CVSTPreset.cpp: implementation for CFxProgram/CFxBank classes (VST SDK 2.4r2).*/
/*****************************************************************************/

/*****************************************************************************/
/* Work Derived from the LGPL host "vsthost (1.16m)".						 */
/* (http://www.hermannseib.com/english/vsthost.htm)"						 */
/* vsthost has the following lincense:										 *

Copyright (C) 2006-2007  Hermann Seib

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/



#include <psycle/host/detail/project.private.hpp>
#include "CVSTPreset.hpp"                   /* private prototypes                */

namespace seib {
	namespace vst {

		/*****************************************************************************/
		/* Static Data                                                               */
		/*****************************************************************************/
		bool CFxBase::NeedsBSwap;

		/*===========================================================================*/
		/* CFxBase class members                                                     */
		/*===========================================================================*/

		CFxBase::CFxBase() {
			CreateInitialized();
		}
		void CFxBase::CreateInitialized()
		{
			fxMagic=0; version = 0; fxID = 0; fxVersion = 0; pf = 0; byteSizePos = 0;
			const char szChnk[] = "CcnK";
			const VstInt32 lChnk = cMagic;
			NeedsBSwap = (memcmp(szChnk, &lChnk, 4) != 0);
			initialized = false;
		}
		CFxBase::CFxBase(VstInt32 _version,VstInt32 _fxID, VstInt32 _fxVersion)
			: fxMagic(0)
			, version(_version)
			, fxID(_fxID)
			, fxVersion(_fxVersion)
			, pf(0)
			, byteSizePos(0)
		{
			const char szChnk[] = "CcnK";
			const VstInt32 lChnk = cMagic;
			NeedsBSwap = (memcmp(szChnk, &lChnk, 4) != 0);
			initialized = false;
		}
		CFxBase::CFxBase(const char *pszFile)
		{
			CreateInitialized();
			if ( Load(pszFile) ) initialized = true;
		}
		CFxBase::CFxBase(FILE* pFileHandle)
		{
			CreateInitialized();
			pf = pFileHandle;
			if (!pf)
				return;
			if ( LoadData() ) initialized = true;
		}
		CFxBase & CFxBase::DoCopy(const CFxBase &org)
		{
			fxMagic=org.fxMagic;
			version=org.version;
			fxID=org.fxID;
			fxVersion=org.fxVersion;
			pathName = org.pathName;
			initialized = org.initialized;
			pf = org.pf;
			byteSizePos = 0;
			return *this;
		}
		void CFxBase::SwapBytes(VstInt32 &l)
		{
			///\todo: could this be improved?
			unsigned char *b = (unsigned char *)&l;
			VstInt32 intermediate =  ((VstInt32)b[0] << 24) |
				((VstInt32)b[1] << 16) |
				((VstInt32)b[2] << 8) |
				(VstInt32)b[3];
			l = intermediate;
		}

		void CFxBase::SwapBytes(float &f)
		{
			VstInt32 *pl = reinterpret_cast<VstInt32 *>(&f);
			SwapBytes(*pl);
		}

		template <class T>
		bool CFxBase::Read(T &f,bool allowswap)	{ size_t i=fread(&f,sizeof(T),1,pf); if (NeedsBSwap && allowswap) SwapBytes(f); return (bool)i; }
		template <class T>
		bool CFxBase::Write(T f, bool allowswap)	{ if (NeedsBSwap && allowswap) SwapBytes(f); size_t i=fwrite(&f,sizeof(T),1,pf);  return (bool)i; }
		bool CFxBase::ReadArray(void* f, VstInt32 size)	{ size_t i=fread(f,size,1,pf); return (bool)i; }
		bool CFxBase::WriteArray(void* f, VstInt32 size)	{ size_t i=fwrite(f,size,1,pf); return (bool)i; }
		bool CFxBase::ReadHeader()
		{
			VstInt32 chunkMagic(0),byteSize(0);
			Read(chunkMagic);
			if (chunkMagic != cMagic)
				return false;
			Read(byteSize);
			Read(fxMagic);
			Read(version);
			Read(fxID);
			Read(fxVersion);
			return true;
		}
		bool CFxBase::WriteHeader()
		{
			Write(cMagic);
			byteSizePos=ftell(pf);
			Write(0);
			Write(fxMagic);
			Write(version);
			Write(fxID);
			Write(fxVersion);
			return true;
		}
		void CFxBase::UpdateSize() 
		{
			VstInt32 tmpPos = ftell(pf);
			fseek(pf, byteSizePos, SEEK_SET);
			Write(tmpPos-byteSizePos);
			fseek(pf, tmpPos, SEEK_SET);
		}

		bool CFxBase::Load(const char *pszFile)
		{
			pf = fopen(pszFile, "rb");
			if (!pf)
				return false;

			bool retval = LoadData();
			fclose(pf);
			pathName=pszFile;
			return retval;
		}
		bool CFxBase::Save(const char *pszFile)
		{
			pf = fopen(pszFile, "wb");
			if (!pf)
				return false;

			bool retval = SaveData();
			fclose(pf);
			pathName=pszFile;
			return retval;
		}


		/*===========================================================================*/
		/* CFxProgram class members                                                  */
		/*===========================================================================*/

		CFxProgram::CFxProgram(VstInt32 _fxID, VstInt32 _fxVersion, VstInt32 size, bool isChunk, void *data)
			:CFxBase(1,_fxID, _fxVersion)
		{
			Init();
			if (!data)
			{
				//Create an amount of memory predefined
				if (isChunk) SetChunkSize(size);
				else SetNumParams(size);
			}
			else
			{
				if (isChunk) SetChunk(data,size);
				else SetParameters(static_cast<const float*>(data),size);
			}
		}
		CFxProgram::CFxProgram(FILE *pFileHandle)
			:CFxBase()
		{
			Init();
			pf = pFileHandle;
			if (!pf)
				return;
			LoadData();
		}
		
		CFxProgram::~CFxProgram()
		{
			FreeMemory();
		}

		void CFxProgram::FreeMemory()
		{
			if (pChunk)
			{
				delete[] pChunk;
				pChunk = 0;
				chunkSize = 0;
			}
			if (pParams)
			{
				delete[] pParams;
				pParams = 0;
				numParams = 0;
			}
		}

		void CFxProgram::Init()
		{
			numParams = chunkSize = 0;
			pParams = 0;
			pChunk = 0;
			memset(prgName,0,sizeof(prgName));
			ParamMode();
		}

		CFxProgram & CFxProgram::DoCopy(const CFxProgram &org)
		{
			//FreeMemory();
			CFxBase::DoCopy(org);
			memcpy(prgName,org.prgName,sizeof(prgName));
			if (org.pChunk)
			{
				SetChunk(org.pChunk,org.chunkSize);
			}
			else
			{
				SetParameters(org.pParams,org.numParams);
			}
			return *this;
		}

		bool CFxProgram::SetParameters(const float* pnewparams,VstInt32 params)
		{
			if (!SetNumParams(params,false))
				return false;
			memcpy(pParams,pnewparams,params*sizeof(float));
			initialized=true;
			return true;
		}
		bool CFxProgram::SetNumParams(VstInt32 nPars, bool initializeData)
		{
			if (nPars <=0 )
				return false;

			ParamMode();
			pParams = new float[nPars];
			numParams = nPars;
			if (pParams && initializeData)
				memset(pParams,0,nPars*sizeof(float));
			return !!pParams;
		}
		bool CFxProgram::SetParameter(VstInt32 nParm, float val)
		{
			if (nParm >= numParams)
				return false;
			if (val < 0.0)
				val = 0.0;
			else if (val > 1.0)
				val = 1.0;
			pParams[nParm] = val;
			return true;
		}
		bool CFxProgram::SetChunk(const void *chunk, VstInt32 size)
		{
			if (!SetChunkSize(size,false))
				return false;

			memcpy(pChunk,chunk,size);
			initialized=true;
			return true;
		}

		bool CFxProgram::SetChunkSize(VstInt32 size,bool initializeData)
		{
			if (size <=0 )
				return false;

			ChunkMode();
			pChunk = new unsigned char[size];
			chunkSize = size;
			if ( pChunk && initializeData )
				memset(pChunk,0,size);
			return !!pChunk;
		}

		bool CFxProgram::LoadData()
		{
			CFxBase::LoadData();
			if (fxMagic == fMagic) ParamMode();
			else if ( fxMagic == chunkPresetMagic) ChunkMode();
			else return false;

			Read(numParams);
			ReadArray(prgName,sizeof(prgName));
			if(IsChunk())
			{
				VstInt32 size;
				Read(size);
				if (!SetChunkSize(size,false))
					return false;
				ReadArray(pChunk,size);
			}
			else
			{
				if (!SetNumParams(numParams,false))
					return false;
				for(VstInt32 i = 0 ; i < numParams ; i++)
					Read(pParams[i]);
			}
			initialized=true;
			return true;
		}
		bool CFxProgram::SaveData()
		{
			CFxBase::SaveData();
			Write(numParams);
			WriteArray(prgName,sizeof(prgName));
			if(IsChunk())
			{
				const VstInt32 size = GetChunkSize();
				Write(size);
				WriteArray(pChunk,size);
			}
			else
			{
				for (VstInt32 i = 0; i < numParams ; i++)
					Write(pParams[i]);
			}
			UpdateSize();
			return true;
		}

		/*===========================================================================*/
		/* CFxBank class members                                                     */
		/*===========================================================================*/

		CFxBank::CFxBank(VstInt32 _fxID, VstInt32 _fxVersion, VstInt32 _numPrograms, bool isChunk, VstInt32 _size, void*_data)
			: CFxBase(2,_fxID, _fxVersion)
		{
			Init();
			if (isChunk)
			{
				SetChunk(_data,_size);
			}
			else
			{
				ProgramMode();
				if ( _data )
				{
					for ( VstInt32 i = 0; i < _numPrograms ; i++)
					{
						CFxProgram& prog = static_cast<CFxProgram*>(_data)[i];
						programs.push_back(prog);
					}
					initialized=true;
				}
				else
				{
					for ( VstInt32 i = 0; i < _numPrograms ; i++)
					{
						CFxProgram prog(_fxID,_fxVersion,_size);
						programs.push_back(prog);
					}
				}
			}
			numPrograms = _numPrograms;
		}

		CFxBank::CFxBank(FILE *pFileHandle)
			:CFxBase()
		{
			Init();
			pf = pFileHandle;
			if (!pf)
				return;
			LoadData();
		}

		/*****************************************************************************/
		/* ~CFxBank : destructor                                                     */
		/*****************************************************************************/

		CFxBank::~CFxBank()
		{
			FreeMemory();                               /* unload all data                   */
		}

		/*****************************************************************************/
		/* Init : initializes all data areas                                         */
		/*****************************************************************************/

		void CFxBank::Init()
		{
			pChunk = 0;
			numPrograms = currentProgram = chunkSize = 0;
			programs.clear();
		}


		/*****************************************************************************/
		/* FreMemory : removes a loaded bank from memory                                */
		/*****************************************************************************/

		void CFxBank::FreeMemory()
		{
			if (pChunk)
				delete[] pChunk;
			//*szFileName = '\0';                     /* reset file name                   */
			pChunk = 0;                           /* reset bank pointer                */
			chunkSize = 0;
			//nBankLen = 0;                           /* reset bank length                 */
			//bChunk = false;                         /* and of course it's no chunk.      */
			programs.clear();
			numPrograms = 0;
			currentProgram = 0;
		}

		/*****************************************************************************/
		/* DoCopy : combined for copy constructor and assignment operator            */
		/*****************************************************************************/

		CFxBank & CFxBank::DoCopy(const CFxBank &org)
		{
			//FreeMemory();
			CFxBase::DoCopy(org);
			if (org.IsChunk())
			{
				SetChunk(org.pChunk,org.chunkSize);
			}
			else
			{
				for (VstInt32 i=0; i < org.numPrograms; i++)
				{
					CFxProgram newprog(org.programs[i]);
					programs.push_back(newprog);
				}
				initialized=true;
			}
			numPrograms=org.numPrograms;
			currentProgram=org.currentProgram;
			return *this;
		}

		/*****************************************************************************/
		/* SetChunk / SetChunkSize : sets a new chunk								 */
		/*****************************************************************************/
		bool CFxBank::SetChunk(const void *chunk, VstInt32 size)
		{
			if (!SetChunkSize(size))
				return false;

			memcpy(pChunk,chunk,size);
			initialized=true;
			return true;
		}

		bool CFxBank::SetChunkSize(VstInt32 size, bool initializeData)
		{
			if (size <=0 )
				return false;

			ChunkMode();
			pChunk = new unsigned char[size];
			chunkSize = size;
			if ( pChunk && initializeData )
				memset(pChunk,0,size);
			return !!pChunk;
		}

		/*****************************************************************************/
		/* LoadBank : loads a bank file                                              */
		/*****************************************************************************/

		bool CFxBank::LoadData()
		{
			CFxBase::LoadData();
			if (fxMagic == bankMagic) ProgramMode();
			else if ( fxMagic == chunkBankMagic) ChunkMode();
			else return false;

			Read(numPrograms);
			if (version == 2) { Read(currentProgram); Forward(124); }
			else { currentProgram = 0; Forward(128); }

			if (fxMagic == chunkBankMagic)
			{
				VstInt32 size;
				Read(size);
				SetChunkSize(size,false);
				ReadArray(pChunk,size);
			}
			else
			{
				for (VstInt32 i=0; i< numPrograms; i++)
				{
					CFxProgram loadprog(pf);
					programs.push_back(loadprog);
				}
			}
			initialized=true;
			return true;
		}

		/*****************************************************************************/
		/* SaveBank : save bank to file                                              */
		/*****************************************************************************/

		bool CFxBank::SaveData()
		{
			CFxBase::SaveData();
			Write(numPrograms);
			if (version == 2) { Write(currentProgram); Forward(124); }
			else Forward(128);

			if (fxMagic == chunkBankMagic)
			{
				VstInt32 size = chunkSize;
				Write(size);
				WriteArray(pChunk,size);
			}
			else
			{
				for (VstInt32 i=0; i< numPrograms; i++)
				{
					programs[i].SaveData(pf);
				}
			}
			UpdateSize();
			return true;
		}
	}
}
