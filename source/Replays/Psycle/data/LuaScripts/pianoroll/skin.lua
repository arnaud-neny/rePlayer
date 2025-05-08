--[[ 
psycle plugineditor (c) 2017 by psycledelics
File: skin.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

return {
  { selector = "patternview",
    properties = {
      selbackgroundcolor = "@PVSELECTION",
      backgroundcolor = "@PVROWBEAT",
      color = "@PVFONT",
      ocatvebackgroundcolor = "@PVROW4BEAT"
    }
  },
  { selector = "gridview",
    properties = {
      eventcolor = "@PVFONT",
      eventplaycolor = "@PVPLAYBAR",
      fonteventcolor = "@PVROW",
      fonteventplaycolor = "@PVFONTPLAY",
      seleventcolor = "@PVSELECTION",      
      fontcolor = "@PVFONT",
      eventblockcolor = "@PVFONTSEL",
      backgroundcolor = "@PVBACKGROUND",
      octaverowcolor = "@PVROWBEAT",
      patternendcolor = "@PVROWBEAT",
      rowcolor = "@PVROW",
      rastercolor = "@PVROWBEAT",
      linebeatcolor = "@PVBACKGROUND",
      line4beatcolor = "@PVROW4BEAT",
      linebarcolor = "@PVROW4BEAT",    
      color = "@PVFONT",
      selbackgroundcolor = "@PVSELECTION",    
      cursorbarcolor = "@PVCURSOR",
      playbarcolor = "@PVPLAYBAR",
      gridbordercolor = "@PVFONT"
    }
  },
  { selector = "trackview",
    properties = {
      cursorcolor = "@PVCURSOR",
      backgroundcolor =  "@PVBACKGROUND",
      fontcolor = "@PVFONT",
      fontcursorcolor = "@PVFONTCURSOR"
    }
  },
  { selector = "trackheader",
    properties = {
      bordercolor =  "@PVROW",
      backgroundcolor =  "@PVBACKGROUND",
      fontcolor = "@PVFONT"
    }
  },  
  { selector = "keyboard",
    properties = {
      backgroundcolor = "@PVBACKGROUND",
      color = "@PVFONT",
      ocatvebackgroundcolor = "@PVROW4BEAT",
      keycolor = "@PVROW",
      keyplaycolor = "@PVPLAYBAR",
      octavekeycolor = "@PVROWBEAT",
      keyhighlightcolor = "@PVPLAYBAR"
    }
  },
  { selector = "controlboard",
    properties = {
      backgroundcolor = "@PVBACKGROUND",
      color = "@PVFONT",
      ocatvebackgroundcolor = "@PVROW4BEAT",
      keycolor = "@PVROW",
      selcmdcolor = "@PVSELECTION"
    }
  },
  { selector = "miniview",
    properties = {
      eventcolor = 0xFFFF00FF,
      backgroundcolor =  "@PVBACKGROUND",      
      playbarcolor = "@PVPLAYBAR",
      scrollbarcolor = "@PVFONT",
      bordercolor = "@PVROW"
    }
  },
  { selector = "switch",
    properties = {
      backgroundcolor =  "@PVBACKGROUND",
      fontcolor = "@PVFONT",
      color = "@PVFONTCURSOR"      
    }
  },
}
