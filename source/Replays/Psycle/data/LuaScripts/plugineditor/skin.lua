--[[ 
psycle plugineditor (c) 2017 by psycledelics
File: skin.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]
return {
  { selector = "textpage",
    properties = {
      backgroundcolor = "@PVROWBEAT",
      font_family = "consolas",
      font_size =  10,
      linenumberforegroundcolor = "@PVFONT",
      linenumberbackgroundcolor = "@PVBACKGROUND",
      blockforegroundcolor = "@PVSELECTION",
      selbackgroundcolor = "@PVSELECTION",
      foldingbackgroundcolor = "@PVBACKGROUND",
      foldingmarkerforecolor = "@PVBACKGROUND",
      foldingmarkerbackcolor = "@PVFONT",
      marginbackgroundcolor = "@PVBACKGROUND",
      backgroundcolor = "@PVBACKGROUND",
      foregroundcolor = "@PVFONT",      
      textcolor = "@PVFONT",
      caretlinebackgroundcolor = "@PVROW",
      caretcolor = "@PVFONT",
      lua_lexer_commentcolor = 0xffb0d8b1,
      lua_lexer_commentlinecolor = 0xffb0d8b1,
      lua_lexer_commentdoccolor = 0xffb0d8b1,
      lua_lexer_foldingmarkerforecolor = 0xff939393,
      lua_lexer_foldingmarkerbackcolor = 0xff939393,
      lua_lexer_operatorcolor = 0xffa6b5e1,
      lua_lexer_wordcolor = 0xffa6b5e1,
      lua_lexer_stringcolor = 0xffa0a0a0,
      lua_lexer_identifiercolor = 0xffcacaca,
      lua_lexer_numbercolor = 0xffffa54b
    }
  },
  { selector = "advancededit",
    properties = {
      backgroundcolor = "@PVROWBEAT",
      font_family = "courier",
      font_size =  10,
      backgroundcolor = "@PVROW",
      hovercolor = "@PVFONT",
      hoverbackgroundcolor = "@PVROW4BEAT",
      activecolor = "@PVFONT",
      activebackgroundcolor = "@PVROW4BEAT"
    }
  },
  { selector = "statusbar",
    properties = {
      backgroundcolor = "@PVROWBEAT"
    }
  },
  { selector = "output",
    properties = {
      backgroundcolor = "@PVROWBEAT"
    }
  },
  { selector = "commandhandler",
    properties = {
      backgroundcolor = "@PVROWBEAT",
      font_family = "Lucida Sans Typewriter",
      font_size =  8
    }
  },
}
