// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2015 members of the psycle project http://psycle.sourceforge.net

///\file
///\brief implementation file for psycle::host::LuaPlugin.

#include <psycle/host/detail/project.private.hpp>
#include <psycle/host/LuaPlugin.hpp>
#include "MainFrm.hpp"
#include "FrameMachine.hpp"

#include <psycle/host/Global.hpp>
#include <psycle/host/Configuration.hpp>
#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/helpers/math.hpp>
#include "Zap.hpp"
#include "Ui.hpp"

#include <lua.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>

namespace psycle { namespace host {
  //////////////////////////////////////////////////////////////////////////
  // Lua

  int LuaPlugin::idex_ = 1024;
   
  LuaPlugin::LuaPlugin(const std::string& dllpath, int index, bool full) : 
      proxy_(this, dllpath),
      curr_prg_(0),      
      do_exit_(false), 
      do_reload_(false),
      usenoteon_(false) {   
    dll_path_ = dllpath;
    _macIndex = (index == -1) ? idex_++ : index;
    _type = MACH_LUA;
    _mode = MACHMODE_FX;    
    // if (full || !proxy().HasDirectMetaAccess()) {
      MachineInits();
    // }
    try {
      proxy().PrepareState();
      proxy().Run();
      if (!proxy().IsPsyclePlugin()) {
        throw std::runtime_error("no psycle plugin"); 
      }
      if (full || !proxy().HasDirectMetaAccess()) {        
        proxy().Start();        
      }      
      _mode = proxy().meta().mode;            
      usenoteon_ = proxy().meta().flags;
      strncpy(_editName, proxy().meta().name.c_str(),sizeof(_editName) - 1);    
      if (full) {
        proxy().Init();
        StartTimer();
      }    
    } catch (...) {
      StopTimer();
      proxy().Free();
      throw;
    }
  }

  void LuaPlugin::MachineInits() {            
    InitializeSamplesVector();        
    for(int i(0) ; i < MAX_TRACKS; ++i) {      
      trackNote[i].key = 255; // No Note.
      trackNote[i].midichan = 0;
    }   
  }

  LuaPlugin::~LuaPlugin() {
    StopTimer();
    Free();    
  }

  void LuaPlugin::Free() {
    try {
      StopTimer();
      proxy().Free();
    } catch(std::exception& e) {
      ui::alert(e.what());
    }
  }

  void LuaPlugin::OnTimerViewRefresh() {    
    if (!do_reload_ && crashed()) {
      return;
    }
    if (!do_reload_) {
	  try {
        proxy().OnTimer();
	  } catch (std::exception& ) {       
        set_crashed(true);
		return; 
      }
    }
    if (do_exit_) {
      HostExtensions::Instance().Remove(this_ptr());
    } else 
    if (do_reload_) {
      do_reload_ = false;
      try {        
        GlobalTimer::instance().KillTimer();    
        reload();        
      } catch (std::exception& ) {
        Mute(false);
        set_crashed(false);  
      }
      GlobalTimer::instance().StartTimer();
    }      
  }

  void LuaPlugin::OnReload() {    
    proxy().Reload();
    if (_macIndex < MAX_MACHINES &&
        ((CMainFrame*)::AfxGetMainWnd())->m_pWndMac[_macIndex]) {
      ((CMainFrame*)::AfxGetMainWnd())->m_pWndMac[_macIndex]->OnReload();
    }    
  }

  int LuaPlugin::GenerateAudioInTicks(int /*startSample*/, int numSamples) throw(psycle::host::exception)
  {
    if (crashed()) {
      return numSamples;
    }
    if(_mode == MACHMODE_GENERATOR) {
      Standby(false);
    }

    if (!_mute)
    {
      if ((_mode == MACHMODE_GENERATOR) || (!Bypass() && !Standby()))
      {
        int ns = numSamples;
        int us = 0;
        while (ns)
        {
          int nextevent = (TWSActive)?TWSSamples:ns+1;
          for (int i=0; i < Global::song().SONGTRACKS; i++)
          {
            if (TriggerDelay[i]._cmd)
            {
              if (TriggerDelayCounter[i] < nextevent)
              {
                nextevent = TriggerDelayCounter[i];
              }
            }
          }
          if (nextevent > ns)
          {
            if (TWSActive)
            {
              TWSSamples -= ns;
            }
            for (int i=0; i < Global::song().SONGTRACKS; i++)
            {
              // come back to this
              if (TriggerDelay[i]._cmd)
              {
                TriggerDelayCounter[i] -= ns;
              }
            }
            try
            {
              //proxy().Work(samplesV[0]+us, samplesV[1]+us, ns, Global::song().SONGTRACKS);
              proxy().Work(ns, us);
            }
            catch(const std::exception & e)
            {
              e;
              return 0;
            }
            ns = 0;
          }
          else
          {
            if(nextevent)
            {
              ns -= nextevent;
              try
              {
                //todo: this should change if we implement multi-io for native plugins (complicated right now. needs new API calls)
                //proxy().Work(samplesV[0]+us, samplesV[1]+us, nextevent, Global::song().SONGTRACKS);
                proxy().Work(nextevent, us);
              }
              catch(const std::exception &e)
              {
                e;
                return 0;
              }
              us += nextevent;
            }
            if (TWSActive)
            {
              if (TWSSamples == nextevent)
              {
                int activecount = 0;
                TWSSamples = TWEAK_SLIDE_SAMPLES;
                for (int i = 0; i < MAX_TWS; i++)
                {
                  if (TWSDelta[i] != 0)
                  {
                    TWSCurrent[i] += TWSDelta[i];

                    if (((TWSDelta[i] > 0) && (TWSCurrent[i] >= TWSDestination[i]))
                      || ((TWSDelta[i] < 0) && (TWSCurrent[i] <= TWSDestination[i])))
                    {
                      TWSCurrent[i] = TWSDestination[i];
                      TWSDelta[i] = 0;
                    }
                    else
                    {
                      activecount++;
                    }
                    try
                    {
                      proxy().ParameterTweak(TWSInst[i], int(TWSCurrent[i])/(double)0xFFFF);
                    }
                    catch(const std::exception &e)
                    {
                      e;
                      return 0;
                    }
                  }
                }
                if(!activecount) TWSActive = false;
              }
            }
            for (int i=0; i < Global::song().SONGTRACKS; i++)
            {
              // come back to this
              if (TriggerDelay[i]._cmd == PatternCmd::NOTE_DELAY)
              {
                if (TriggerDelayCounter[i] == nextevent)
                {
                  // do event
                  try
                  {
                    proxy().SeqTick(i ,TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
                  }
                  catch(const std::exception &e)
                  {
                    e;
                    return 0;
                  }
                  TriggerDelay[i]._cmd = 0;
                }
                else
                {
                  TriggerDelayCounter[i] -= nextevent;
                }
              }
              else if (TriggerDelay[i]._cmd == PatternCmd::RETRIGGER)
              {
                if (TriggerDelayCounter[i] == nextevent)
                {
                  // do event
                  try
                  {
                    proxy().SeqTick(i, TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
                  }
                  catch(const std::exception &e)
                  {
                    e;
                    return 0;
                  }
                  TriggerDelayCounter[i] = (RetriggerRate[i]*Global::player().SamplesPerRow())/256;
                }
                else
                {
                  TriggerDelayCounter[i] -= nextevent;
                }
              }
              else if (TriggerDelay[i]._cmd == PatternCmd::RETR_CONT)
              {
                if (TriggerDelayCounter[i] == nextevent)
                {
                  // do event
                  try
                  {
                    proxy().SeqTick(i ,TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
                  }
                  catch(const std::exception &e)
                  {
                    e;
                    return 0;
                  }
                  TriggerDelayCounter[i] = (RetriggerRate[i]*Global::player().SamplesPerRow())/256;
                  int parameter = TriggerDelay[i]._parameter&0x0f;
                  if (parameter < 9)
                  {
                    RetriggerRate[i]+= 4*parameter;
                  }
                  else
                  {
                    RetriggerRate[i]-= 2*(16-parameter);
                    if (RetriggerRate[i] < 16)
                    {
                      RetriggerRate[i] = 16;
                    }
                  }
                }
                else
                {
                  TriggerDelayCounter[i] -= nextevent;
                }
              }
              else if (TriggerDelay[i]._cmd == PatternCmd::ARPEGGIO)
              {
                if (TriggerDelayCounter[i] == nextevent)
                {
                  PatternEntry entry =TriggerDelay[i];
                  switch(ArpeggioCount[i])
                  {
                  case 0:
                    try
                    {
                      proxy_.SeqTick(i ,TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
                    }
                    catch(const std::exception &e)
                    {
                      e;
                      return 0;
                    }
                    ArpeggioCount[i]++;
                    break;
                  case 1:
                    entry._note+=((TriggerDelay[i]._parameter&0xF0)>>4);
                    try
                    {
                      proxy_.SeqTick(i ,entry._note, entry._inst, 0, 0);
                    }
                    catch(const std::exception &e)
                    {
                      e;
                      return 0;
                    }
                    ArpeggioCount[i]++;
                    break;
                  case 2:
                    entry._note+=(TriggerDelay[i]._parameter&0x0F);
                    try
                    {
                      proxy_.SeqTick(i ,entry._note, entry._inst, 0, 0);
                    }
                    catch(const std::exception &e)
                    {
                      e;
                      return 0;
                    }
                    ArpeggioCount[i]=0;
                    break;
                  }
                  TriggerDelayCounter[i] = Global::player().SamplesPerTick();
                }
                else
                {
                  TriggerDelayCounter[i] -= nextevent;
                }
              }
            }
          }
        }
        UpdateVuAndStanbyFlag(numSamples);
      }
    }
    else Standby(true);
    return numSamples;
  }

  bool LuaPlugin::LoadSpecificChunk(RiffFile* pFile, int version)
  {
    if (proxy_.prsmode() == MachinePresetType::NATIVE) {
    uint32_t size;
    pFile->Read(size); // size of whole structure
    if(size)
    {
      uint32_t count;
      pFile->Read(count);  // size of vars
      //Read vals and names to do SetParameter.
      //It is done this way to allow parameters to change without disrupting the loader.
      std::vector<int> vals;
      std::map<std::string, int> ids;
      for (uint32_t i = 0; i < count; i++) {
        int temp;
        pFile->Read(temp);
        vals.push_back(temp);
      }
      for (uint32_t i = 0; i < count; i++) {
        std::string id;
        pFile->ReadString(id);
        ids[id] = i;
      }
      int num = GetNumParams();
      for (int i = 0; i < num; ++i) {
        std::string id = proxy_.Id(i);
        std::map<std::string, int>::iterator it = ids.find(id);
        if (it != ids.end()) {
          int idx = ids[id];
          SetParameter(i, vals[idx]);
        } else {
          // parameter not found
        }
      }
      uint32_t size2=0;
      pFile->Read(size2);
      if(size2)
      {
        byte* pData = new byte[size2];
        pFile->Read(pData, size2); // Number of parameters
        try
        {
          proxy().PutData(pData, size2);
          delete[] pData;
        }
        catch(const std::exception &e)
        {
          e;
          delete[] pData;
          return false;
        }
      }
    }
    return true;
    } else {
      try {
					UINT size;
					unsigned char _program;
					pFile->Read(&size, sizeof size );
					if(size)
					{
						UINT count;
						pFile->Read(&_program, sizeof _program);
						pFile->Read(&count, sizeof count);
						size -= sizeof _program + sizeof count + sizeof(float) * count;
						if(!size)
						{
							/*BeginSetProgram();
							SetProgram(_program);
							for(UINT i(0) ; i < count ; ++i)
							{
								float temp;
								pFile->Read(&temp, sizeof temp);
								SetParameter(i, temp);
							}
							EndSetProgram();*/
						}
						else
						{
							/*BeginSetProgram();
							SetProgram(_program);
							EndSetProgram();*/
							pFile->Skip(sizeof(float) *count);
              bool b = proxy_.prsmode() == MachinePresetType::CHUNK;
							if(b)
							{
								unsigned char * data(new unsigned char[size]);
								pFile->Read(data, size); // Number of parameters
                proxy().PutData(data, size);
								zapArray(data);
							}
							else
							{
								// there is a data chunk, but this machine does not want one.
								pFile->Skip(size);
								return false;
							}
						}
					}
					return true;
				}
				catch(...){return false;}
    }
  }

  void LuaPlugin::SaveSpecificChunk(RiffFile * pFile)
  {
    if (proxy_.prsmode() == MachinePresetType::NATIVE) {
      uint32_t count = GetNumParams();
      uint32_t size2(0);
      unsigned char * pData = 0;
      try
      {
        size2 = proxy().GetData(&pData, false);
      }
      catch(const std::exception&)
      {
        // data won't be saved
      }
      uint32_t size = size2 + sizeof(count) + sizeof(int)*count;
      std::vector<std::string> ids;
      for (UINT i = 0; i < count; i++) {
        std::string id = proxy_.Id(i);
        ids.push_back(id);
        size += id.length()+1;
      }
      pFile->Write(size);
      pFile->Write(count);
      for (uint32_t i = 0; i < count; i++)
      {
        int temp = GetParamValue(i);
        pFile->Write(temp);
      }
      // ids
      for (uint32_t i = 0; i < count; i++) {
        pFile->WriteString(ids[i]);
      }
      pFile->Write(size2);
      if(size2)
      {
        pFile->Write(pData, size2); // Number of parameters
        zapArray(pData);
      }
    } else {
      try {
			    UINT count(GetNumParams());
					unsigned char _program=0;
					UINT size(sizeof _program + sizeof count);
					UINT chunksize(0);
					unsigned char * pData(0);
          bool b = proxy_.prsmode() == MachinePresetType::CHUNK;
					if(b)
					{
						count=0;
            chunksize = proxy().GetData(&pData, true);
						size+=chunksize;
					}
					else
					{
						 size+=sizeof(float) * count;
					}
					pFile->Write(&size, sizeof size);
					// _program = static_cast<unsigned char>(GetProgram());
					pFile->Write(&_program, sizeof _program);
					pFile->Write(&count, sizeof count);

					if(b)
					{
						pFile->Write(pData, chunksize);
					}
					else
					{
						for(UINT i(0); i < count; ++i)
						{
              float temp = proxy_.Val(i);
							pFile->Write(&temp, sizeof temp);
						}
					}
				} catch(...) {
			}
    }
  }

  bool LuaPlugin::SetParameter(int numparam, int value) {
    if (crashed() || numparam < 0) {
      return false;
    }
    try {
      int minval; int maxval;
      proxy().Range(numparam, minval, maxval);
      int quantization = (maxval - minval);
      proxy().ParameterTweak(numparam, value / static_cast<double>(quantization));
      return true;
    } catch(std::exception&) {} //do nothing.
    return false;
  }

  void LuaPlugin::GetParamRange(int numparam,int &minval, int &maxval) {
    if (crashed() || numparam < 0) {
      minval = 0;
      maxval = 0xFFFF;
      return;
    }
    try {
      if( numparam < GetNumParams() ) {
        proxy().Range(numparam, minval, maxval);
      }
    } catch(std::exception&) {}
  }

  int LuaPlugin::GetParamType(int numparam) {
    if (crashed() || numparam < 0) {
      return 0;
    }
    int mpf = 0 ;
    try {
      if( numparam < GetNumParams() ) {
        mpf = proxy().Type(numparam);
      }
    } catch(std::exception&) {}
    return mpf;
  }

  void LuaPlugin::GetParamName(int numparam, char * parval) {
    if (crashed() || numparam < 0) {
      std::strcpy(parval, "");
      return;
    }
    try {
      if( numparam < GetNumParams() ) {
        std::string name = proxy().Name(numparam);
        std::strcpy(parval, name.c_str());
      } else std::strcpy(parval, "Out of Range");
    } catch(std::exception &e) { e; std::strcpy(parval, ""); }
  }

  int LuaPlugin::GetParamValue(int numparam){
    if (crashed() || numparam < 0) {
      return 0;
    }
    if (numparam < GetNumParams()) {
      int minval; int maxval;
      try {
        proxy_.Range(numparam, minval, maxval);
        int quantization = (maxval - minval);
        return proxy().Val(numparam) * quantization;
      } catch(std::exception&) {} //do nothing.
    } else {
      // out of range
    }
    return 0;
  }
  
  void LuaPlugin::GetParamValue(int numparam, char * parval) {
    if (crashed() || numparam < 0) {
      std::strcpy(parval, "Out of Range or Crashed");
      return;
    }
    if(numparam < GetNumParams()) {
      try {
        if(!proxy().DescribeValue(numparam, parval)) {
          std::sprintf(parval,"%.0f",GetParamValue(numparam) * 1); // 1 = Plugin::quantizationVal())
        }
      }
      catch(const std::exception &e) {
        e;
        return;
      }
    }
    else std::strcpy(parval,"Out of Range");
  }

  void LuaPlugin::GetParamId(int numparam, std::string& id) {
    if (crashed() || numparam < 0) {
      return;
    }
    if(numparam < GetNumParams()) {
      try {
        id = proxy_.Id(numparam);
      } catch(const std::exception &e) {
        e;
        return;
      }
    }
  }

  void LuaPlugin::AfterTweaked(int numparam) {
    if (crashed() || numparam < 0) {
      return;
    }
    if(numparam < GetNumParams()) {
      try {
        proxy_.call_aftertweaked(numparam);
      } catch(const std::exception&) {}
    }
  }

  std::string LuaPlugin::help() {
    if (crashed()) {
      return "Plugin crashed. No help available.";
    }
    try {
      return proxy_.call_help();
    } catch(const std::exception &) { }
    return "";
  }

  void LuaPlugin::NewLine() {
    if (crashed()) {
      return;
    }
    try {
      proxy().SequencerTick();
    } catch(const std::exception &e) { e; }
  }

  void LuaPlugin::Tick(int channel, PatternEntry * pData){
    if (crashed()) {
      return;
    }
    if(pData->_note == notecommands::tweak || pData->_note == notecommands::tweakeffect)
    {
		int param = translate_param(pData->_inst);
      if(param < proxy_.num_parameter())
      {
        int nv = (pData->_cmd<<8)+pData->_parameter;
        int const min = 0; // always range 0 .. FFFF like vst tweak
        int const max = 0xFFFF;
        nv += min;
        if(nv > max) nv = max;
        // quantization done in parameter.lua
        try
        {
          proxy().ParameterTweak(param, double(nv)/0xFFFF);
        }
        catch(const std::exception &e)
        {
          e;
          return;
        }
      }
    } else if(pData->_note == notecommands::tweakslide)
    {
		int param = translate_param(pData->_inst);
      if(param < proxy_.num_parameter())
      {
        int i;
        if(TWSActive)
        {
          // see if a tweak slide for this parameter is already happening
          for(i = 0; i < MAX_TWS; i++)
          {
            if((TWSInst[i] == param) && (TWSDelta[i] != 0))
            {
              // yes
              break;
            }
          }
          if(i == MAX_TWS)
          {
            // nope, find an empty slot
            for (i = 0; i < MAX_TWS; i++)
            {
              if (TWSDelta[i] == 0)
              {
                break;
              }
            }
          }
        }
        else
        {
          // wipe our array for safety
          for (i = MAX_TWS-1; i > 0; i--)
          {
            TWSDelta[i] = 0;
          }
        }
        if (i < MAX_TWS)
        {
          TWSDestination[i] = float(pData->_cmd<<8)+pData->_parameter;
          float min = 0; // float(_pInfo->Parameters[pData->_inst]->MinValue);
          float max = 0xFFFF; //float(_pInfo->Parameters[pData->_inst]->MaxValue);
          TWSDestination[i] += min;
          if (TWSDestination[i] > max)
          {
            TWSDestination[i] = max;
          }
          TWSInst[i] = param;
          try
          {
            TWSCurrent[i] = proxy_.Val(TWSInst[i])*0xFFFF;
          }
          catch(const std::exception &e)
          {
            e;
            return;
          }
          TWSDelta[i] = float((TWSDestination[i]-TWSCurrent[i])*TWEAK_SLIDE_SAMPLES)/Global::player().SamplesPerRow();
          TWSSamples = 0;
          TWSActive = TRUE;
        }
        else
        {
          // we have used all our slots, just send a twk
          int nv = (pData->_cmd<<8)+pData->_parameter;
          int const min = 0; //_pInfo->Parameters[pData->_inst]->MinValue;
          int const max = 0xFFFF; //_pInfo->Parameters[pData->_inst]->MaxValue;
          nv += min;
          if (nv > max) nv = max;
          // quantization done in parameter.lua
          try
          {
            proxy().ParameterTweak(param, nv/(double)0xFFFF);
          }
          catch(const std::exception &e)
          {
            e;
            return;
          }
        }
      }
    } else
      try {
        const int note = pData->_note;
        if (usenoteon_==0) {
          proxy().SeqTick(channel, pData->_note, pData->_inst, pData->_cmd,
            pData->_parameter);
        } else {
          // noteon modus
          if (note < notecommands::release)  { // Note on
            SendNoteOn(channel, note, pData->_inst, pData->_cmd, pData->_parameter);
          } else if (note == notecommands::release) { // Note Off.
            SendNoteOff(channel, note, notecommands::empty, pData->_inst,
              pData->_cmd, pData->_parameter);
          } else {
            SendCommand(channel, pData->_inst, pData->_cmd, pData->_parameter);
          }
        }
    } catch(const std::exception &e) { e; }
  }

  void LuaPlugin::Stop(){
    if (crashed()) {
      return;
    }
    try {
      proxy().Stop();
      if (usenoteon_!=0) {
        for(int i(0) ; i < MAX_TRACKS; ++i) {
          trackNote[i].key = 255; // No Note.
          trackNote[i].midichan = 0;
        }
      }
    } catch(const std::exception &e) { e; }
  }

  // additions if noteon mode is used
  void LuaPlugin::SendCommand(unsigned char channel,
    unsigned char inst,
    unsigned char cmd,
    unsigned char val) {
      int oldnote = notecommands::empty;
      if(trackNote[channel].key != notecommands::empty) {
        oldnote = trackNote[channel].key;
      }
      proxy().Command(oldnote, inst, cmd, val);
  }

  void LuaPlugin::SendNoteOn(unsigned char channel,
    unsigned char key,
    unsigned char inst,
    unsigned char cmd,
    unsigned char val) {
      int oldnote = notecommands::empty;
      if(trackNote[channel].key != notecommands::empty) {
        oldnote = trackNote[channel].key;
        SendNoteOff(channel, trackNote[channel].key, oldnote, inst, cmd, val);
      }
      note thisnote;
      thisnote.key = key;
      thisnote.midichan = 0;
      trackNote[channel] = thisnote;
      proxy().NoteOn(key, oldnote, inst, cmd, val);
  }

  void LuaPlugin::SendNoteOff(unsigned char channel,
    unsigned char key,
    unsigned char lastkey,
    unsigned char inst,
    unsigned char cmd,
    unsigned char val) {
      if (trackNote[channel].key == notecommands::empty)
        return;
      note thenote = trackNote[channel];
      proxy().NoteOff(thenote.key, lastkey, inst, cmd, val);
      trackNote[channel].key = 255;
      trackNote[channel].midichan = 0;
  }

  //Bank & Programs

   void LuaPlugin::SetCurrentProgram(int idx) {
     if (crashed()) {
       return;
     }
     try {
       proxy_.call_setprogram(idx);
       curr_prg_ = idx;
     } catch(const std::exception &e) { e; }
   }

   void LuaPlugin::GetIndexProgramName(int bnkidx, int prgIdx, char* val) {
     if (crashed()) {
       std::strcpy(val, "");
       return;
     }
     try {
       std::string name = proxy_.get_program_name(bnkidx, prgIdx);
       std::strcpy(val, name.c_str());
     } catch(const std::exception &e) {
       e; std::strcpy(val, "Out of Range");
     }
   }

   //Bank & Programs
   int LuaPlugin::GetNumPrograms() {
     if (crashed()) {
       return 0;
     }
     try {
       return proxy_.call_numprograms();
     } catch(const std::exception &e) { e; }
     return 0;
   }

   int LuaPlugin::GetCurrentProgram() {
     if (crashed()) {
       return 0;
     }
     try {
       return proxy_.get_curr_program();
     } catch(const std::exception &e) { e; }
     return 0;
   }

   void LuaPlugin::SaveBank(const std::string& filename) {
      unsigned char * pData(0);
      int chunksize = proxy().GetData(&pData, true);
      using namespace std;
	  #if __cplusplus >= 201103L
        ofstream ofile(filename, ios::binary);
      #else
	    ofstream ofile(filename.c_str(), ios::binary);
      #endif
      ofile.write((char*)pData, chunksize);
      ofile.close();
   }

   bool LuaPlugin::LoadBank(const std::string& filename) {
      using namespace std;
      streampos size;
      char* pData;
	  #if __cplusplus >= 201103L
        ifstream file (filename, ios::in|ios::binary|ios::ate);
      #else
	    ifstream file (filename.c_str(), ios::in|ios::binary|ios::ate);
      #endif
      if (file.is_open()) {
        size = file.tellg();
        pData = new char[size];
        file.seekg (0, ios::beg);
        file.read(pData, size);
        file.close();
        proxy().PutData((unsigned char*)pData, size);
        delete[] pData;
        return true;
      }
      return false;
   }
}   // namespace
}   // namespace