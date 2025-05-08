/*****************************************************************************/
/* CVSTPreset.hpp: interface for CFxProgram/CFxBank (for VST SDK 2.4r2). */
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

#pragma once
// steinberg's headers are unable to correctly detect the platform,
// so we need to detect the platform ourselves,
// and declare steinberg's unstandard/specific options: WIN32/MAC
#if defined DIVERSALIS__OS__MICROSOFT
	#if !defined WIN32
		#define WIN32 // steinberg's build option
	#endif
#elif defined DIVERSALIS__OS__APPLE
	#if !defined MAC
		#define MAC // steinberg's build option
	#endif
#else
	#error "internal steinberg error"
#endif

/// Tell the SDK that we want to support all the VST specs, not only VST2.4
#define VST_FORCE_DEPRECATED 0

// VST header files
#include <vst2.x/aeffectx.h>
#include <vst2.x/vstfxstore.h>


namespace seib { namespace vst {

		/*****************************************************************************/
		/* CFxBase : base class for FX Bank / Program Files                          */
		/*****************************************************************************/
		class PSYCLE__CORE__DECL CFxBase
		{
		protected:
			CFxBase();
		public:
			CFxBase(VstInt32 _version,VstInt32 _fxID, VstInt32 _fxVersion);
			CFxBase(const char *pszFile);
			CFxBase(FILE* pFileHandle);
			CFxBase & operator=(CFxBase const &org) { return DoCopy(org); }
			CFxBase & DoCopy(const CFxBase &org);

			bool Save(const char *pszFile);
			VstInt32 GetMagic() const { return fxMagic; }
			VstInt32 GetVersion() const { return version; }
			VstInt32 GetFxID() const {  return fxID; }
			VstInt32 GetFxVersion() const { return fxVersion; }
			bool Initialized() const { return initialized; }
			std::string GetPathName() const { return pathName; }
			// This function would normally be protected, but it is needed in the saving of Programs from a Bank.
			virtual bool SaveData(FILE* pFileHandle) { pf = pFileHandle; return SaveData(); }
		protected:
			VstInt32 fxMagic;			///< "Magic" identifier of this chunk. Tells if it is a Bank/Program and if it is chunk based.
			VstInt32 version;			///< format version
			VstInt32 fxID;				///< fx unique ID
			VstInt32 fxVersion;			///< fx version
			std::string pathName;
			bool initialized;
			// pf would normally be private, but it is needed in the loading of Programs from a Bank.
			FILE* pf;
		protected:
			bool Load(const char *pszFile);
			virtual bool LoadData() { return ReadHeader(); }
			virtual bool SaveData() { return WriteHeader(); }
			template <class T>
			bool Read(T &f,bool allowswap=true);
			template <class T>
			bool Write(T f,bool allowswap=true);
			bool ReadArray(void *f, VstInt32 size);
			bool WriteArray(void *f, VstInt32 size);
			void Rewind(VstInt32 bytes) { fseek(pf,-bytes,SEEK_CUR); }
			void Forward(VstInt32 bytes) { fseek(pf,bytes,SEEK_CUR); }
			bool ReadHeader();
			bool WriteHeader();
			void UpdateSize();
			virtual void CreateInitialized();
		private:
			static bool NeedsBSwap;
			VstInt32 byteSizePos;
		private:
			void SwapBytes(VstInt32 &f);
			void SwapBytes(float &f);
		};

		/*****************************************************************************/
		/* CFxProgram : class for an .fxp (Program) file                             */
		/*****************************************************************************/
		class PSYCLE__CORE__DECL CFxProgram : public CFxBase
		{
		public:
			CFxProgram(const char *pszFile = 0):CFxBase(){ Init(); initialized=Load(pszFile); }
			CFxProgram(FILE *pFileHandle);
			// Create a CFxProgram from parameters.
			// _fxID and _fxVersion are mandatory. 
			// if isChunk == false, size is the number of parameters, and data, if not empty, contains 
			// an array of floats that correspond to the parameter values.
			// if isChunk == true, size is the size of the chunk, and data, if not empty, contains the chunk data.
			CFxProgram(VstInt32 _fxID, VstInt32 _fxVersion, VstInt32 size, bool isChunk=false, void *data=0);
			CFxProgram(CFxProgram const &org):CFxBase(){ Init(); DoCopy(org); }
			virtual ~CFxProgram();
			CFxProgram & operator=(CFxProgram const &org) { FreeMemory(); return DoCopy(org); }

			// access functions
			const char * GetProgramName() const { return prgName; }
			void SetProgramName(const char *name = "")
			{
				//leave last char for null.
				std::strncpy(prgName, name, 27);
			}
			VstInt32 GetNumParams() const{ return numParams; }
			float GetParameter(VstInt32 nParm) const{  return (nParm < numParams) ? pParams[nParm] : 0; }
			bool SetParameter(VstInt32 nParm, float val = 0.0);
			VstInt32 GetChunkSize() const{ return chunkSize; }
			const void *GetChunk() const { return pChunk; }
			bool CopyChunk(const void *chunk,const VstInt32 size) {	ChunkMode(); return SetChunk(chunk,size);	}
			bool IsChunk() const { return fxMagic == chunkPresetMagic; }
			void ManuallyInitialized() { initialized = true; }

			virtual bool SaveData(FILE* pFileHandle) { return CFxBase::SaveData(pFileHandle); }

		protected:
			char prgName[28];			///< program name (null-terminated ASCII string)

			VstInt32 numParams;			///< number of parameters
			float* pParams;				///< variable sized array with parameter values
			VstInt32 chunkSize;				///< Size of the opaque chunk.
			unsigned char* pChunk;		///< variable sized array with opaque program data

		protected:
			void Init();
			CFxProgram & DoCopy(CFxProgram const &org);
			void FreeMemory();
			virtual bool LoadData();
			virtual bool SaveData();

		private:
			void ChunkMode() { FreeMemory(); fxMagic = chunkPresetMagic; }
			void ParamMode() { FreeMemory(); fxMagic = fMagic; }
			bool SetParameters(const float* pnewparams,VstInt32 params);
			bool SetChunk(const void *chunk, VstInt32 size);
			bool SetNumParams(VstInt32 nPars,bool initializeData=true);
			bool SetChunkSize(VstInt32 size,bool initializeData=true);
		};


		/*****************************************************************************/
		/* CFxBank : class for an .fxb (Bank) file                                   */
		/*****************************************************************************/

		class PSYCLE__CORE__DECL CFxBank : public CFxBase
		{
		public:
			CFxBank(const char *pszFile = 0):CFxBase(){ Init(); initialized=Load(pszFile); }
			CFxBank(FILE* pFileHandle);
			// Create a CFxBank from parameters.
			// _fxID, _fxVersion and _numPrograms are mandatory.
			// If isChunk == false, the Bank is created as a regular bank of numPrograms, _size is then the
			// number of parameters of the program, and data can be zero, or an array of CFxPrograms to copy to the CFxBank.
			// if isChunk == true ,create a chunk of size "_size", and copy the contents of "data".
			CFxBank(VstInt32 _fxID, VstInt32 _fxVersion, VstInt32 _numPrograms, bool isChunk=false, VstInt32 _size=0, void *_data=0);
			CFxBank(CFxBank const &org) { Init(); DoCopy(org); }
			virtual ~CFxBank();
			CFxBank & operator=(CFxBank const &org) { FreeMemory(); return DoCopy(org); }

			// access functions
			VstInt32 GetNumPrograms() const { return numPrograms; }
			
			VstInt32 GetChunkSize() const { return chunkSize; }
			void * const GetChunk() const { return pChunk; }
			bool CopyChunk(const void *chunk,const VstInt32 size) {	ChunkMode(); return SetChunk(chunk,size);	}
			bool IsChunk() const{ return fxMagic == chunkBankMagic; }

			// if nProgNum is not specified (i.e, it is -1) , currentProgram is used as index.
			CFxProgram& GetProgram(VstInt32 nProgNum=-1)
			{
				if ( nProgNum < 0 || nProgNum >= numPrograms) return programs[currentProgram];
				else return programs[nProgNum];
			}
			void SetProgramIndex(VstInt32 nProgNum) { if (nProgNum < numPrograms ) currentProgram = nProgNum; }
			VstInt32 GetProgramIndex() const { return currentProgram; }
			void ManuallyInitialized() { initialized = true; }
			virtual bool SaveData(FILE* pFileHandle) { return CFxBase::SaveData(pf); }

		protected:
			VstInt32 numPrograms;
			VstInt32 currentProgram;
			VstInt32 chunkSize;
			unsigned char * pChunk;
			std::vector<CFxProgram> programs;

		protected:
			void Init();
			CFxBank & DoCopy(CFxBank const &org);
			bool SetChunk(const void *chunk, VstInt32 size);
			bool SetChunkSize(VstInt32 size, bool initializeData=true);
			void FreeMemory();
			virtual bool LoadData();
			virtual bool SaveData();

		private:
			void ChunkMode() { FreeMemory(); fxMagic = chunkBankMagic; }
			void ProgramMode() { FreeMemory(); fxMagic = bankMagic; }

		};
}}
