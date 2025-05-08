--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  standarduisheet.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

return {
  { selector = "group",
    properties = {
      color = "@PVFONT",
      font_family = "courier",
      font_size = 12
    }
  },
  { selector = "canvas",
    properties = {
      backgroundcolor = "@PVBACKGROUND"
    }
  },
  { selector = "edit",    
    properties = {      
      backgroundcolor = "@PVROW"
    }
  },
  { selector = "listview",
    properties = {
      backgroundcolor = "@PVBACKGROUND"
    }
  },  
  { selector = "treeview",
    properties = {      
      backgroundcolor = "@PVROWBEAT"
    }
  },
  { selector = "toolicon",
    properties = {      
      backgroundcolor = "@PVBACKGROUND",
      hovercolor = "@PVFONT",
      hoverbackgroundcolor = "@PVROW4BEAT",
      activecolor = "@PVFONT",
      activebackgroundcolor = "@PVSELECTION"
    }
  },  
  { selector = "tabgroup",
    properties = {
      backgroundcolor = "@PVROW",
      tabbarcolor = "@PVBACKGROUND",     
      headercolor = "@PVFONT",
      headerbackgroundcolor = "@PVBACKGROUND",
      headerhovercolor = "@PVSELECTION",
      headerhoverbackgroundcolor = "@PVROW4BEAT",
      headeractivecolor = "@PVFONT",
      headeractivebackgroundcolor = "@PVROW4BEAT",
      headerbordercolor = "@PVSELECTION",
      headerclosecolor = 0xffb0d8b1,
      headerclosehovercolor = 0xffffffff,
      headerclosehoverbackgroundcolor = 0xffa8444c
    }
  },
  { selector = "settingspage",
    properties = {      
      backgroundcolor = "@PVBACKGROUND",     
    }
  }, 
}
