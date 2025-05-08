--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  mainviewport.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local dimension = require("psycle.ui.dimension")
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local viewport = require("psycle.ui.viewport")
local titlebaricons = require("psycle.ui.titlebaricons")
local text = require("psycle.ui.text")
local gridview = require("gridview")
local trackgroup = require("trackgroup")
local hostlistener = require("psycle.ui.hostactionlistener")
local editcommands = require("editcommands")
local selectioncommands = require("selectioncommands")
local toolicon = require("psycle.ui.toolicon")
local toolbar = require("psycle.ui.toolbar")
local player = require("psycle.player"):new()
local sequencebar = require("psycle.sequencebar")
local sequence = require("sequence")
local playtimer = require("playtimer")
local scroller = require("scroller")
local cursor = require("trackcursor")
local inputhandler = require("inputhandler")
local chordselector = require("chordselector")
local keymap = require("keymap")
local gridgroup = require("gridgroup")
local multicommand = require("multicommand")
local redrawgridcommand = require("redrawgridcommand")
local redrawcommand = require("redrawcommand")

local icondir = require("psycle.config"):new("PatternVisual"):luapath() .. "\\psycle\\ui\\icons\\"
local transparency = 0xFFFFFF

local mainviewport = viewport:new()

function mainviewport:new()
  local m = viewport:new()
  setmetatable(m, self)
  self.__index = self    
  m:init()
  return m
end

function mainviewport:init()
  self:invalidatedirect()
  self:linkstyles()
  self.sequence = sequence:new()
                          :bindtohost()                                        
  self.cursor = cursor:new(self.sequence)
  self.scroller = scroller:new()
  self.keymap = keymap:new()
  self.playtimer = playtimer:new(self.sequence)
  self:inittoolbars()
  self.trackgroup = trackgroup:new(self, self, self.keymap, self.sequence, self.cursor, self.scroller)
                                      :addstatuslistener(self)
  self.chordselector = chordselector:new(self)
                                    :setalign(alignstyle.RIGHT)
                                    :addlistener(self)                                    
                                    :addstatuslistener(self)
                                    :hide()                                    
  self.gridgroup = gridgroup:new(self, self.sequence, self.scroller, self.keymap, self.cursor, self.playtimer)
                            :setalign(alignstyle.CLIENT)
                            :setchord(self.chordselector:chord()) 
                            :addstatuslistener(self)  
  self:inithostlistener()
  self:initcommands()  
  self:initplaytimer()
  self.inputhandler = inputhandler:new(self.gridgroup,
                                       self.sequence,
                                       self.cursor,
                                       self.scroller)
                                  :setchord(self.chordselector:chord())
  self.chordselector:addlistener(self.inputhandler)
  self:initsequence()  
end

function mainviewport:linkstyles()
  self:link(require("psycle.ui.standarduisheet"))
      :link(require("skin")) 
end

function mainviewport:initsequence()
  self.sequence:addlistener(self.gridgroup.patternview)
               :addlistener(self.ruler)
               :addlistener(self.gridgroup.controlgroup.controlview)
               :addlistener(self.trackgroup.miniview)
               :addlistener(self)
  self.cursor:setseqpos(self.sequencebar:editposition() + 1)                 
  self.scroller:scrolltoseqpos(self.sequencebar:editposition() + 1)               
end

function mainviewport:inittoolbars()
  self.toolbar = toolbar:new(self)
                        :setalign(alignstyle.TOP)
                        :setautosize(false, true)
                        :setmargin(boxspace:new(0, 0, 5, 0))
  self.titlebaricons = titlebaricons:new(self.toolbar)
                                    :setalign(alignstyle.RIGHT)
                                    :setautosize(false, false)
  self.titlebaricons.settingsicon:hide()  
  self.statustext = text:new(self.toolbar)
                        :setalign(alignstyle.CLIENT)
                        :setautosize(false, false)
                        :setmargin(boxspace:new(0, 50, 0, 50))
                        :setverticalalignment(alignstyle.CENTER)
                        :setjustify(text.RIGHTJUSTIFY)                             
  self:initedittoolbar(self.toolbar)
  self:initdragtoolbar(self.toolbar)
  self:initoptiontoolbar(self.toolbar)  
end

function mainviewport:initplaytimer()
  self.playtimer
      :addlistener(self.gridgroup.ruler)  
      :addlistener(self.gridgroup.patternview)
      :addlistener(self.gridgroup.controlgroup.controlview)
      :addlistener(self.trackgroup.trackview)
      :addlistener(self.ruler)
      :addlistener(self.gridgroup.keyboard)
      :addlistener(self.trackgroup.miniview)
  gridview.setplaytimer(self.playtimer)
  self.trackgroup.miniview:setplaytimer(self.playtimer)
  self.playtimer:start()
end

function mainviewport:initcommands()
  local that = self
  self.editcommands_ = {
    selectnote = editcommands.build(editcommands.SELECTNOTE, that.trackgroup.trackview),
    editmode = editcommands.build(editcommands.EDITMODE, that.gridgroup.patternview, that, that.editicons),
    selectmode = editcommands.build(editcommands.SELECTMODE, that.gridgroup.patternview, that),
    slicemode = editcommands.build(editcommands.SLICEMODE, that.gridgroup.patternview, that),
    copymode = editcommands.build(editcommands.COPYMODE, that.gridgroup.patternview),
    movemode = editcommands.build(editcommands.MOVEMODE, that.gridgroup.patternview),
    trackviewmode = editcommands.build(editcommands.TRACKVIEWMODE, that.gridgroup.patternview),
    trackeditmode = editcommands.build(editcommands.TRACKEDITMODE, that.trackgroup.trackview),
    keyboardmode = editcommands.build(editcommands.KEYBOARDMODE, that.gridgroup.keyboard, that.gridgroup.keyboardmodeselect),
    hzoomfit = editcommands.build(editcommands.HZOOMFIT, that.gridgroup.patternview),
    hzoomin = editcommands.build(editcommands.HZOOMIN, that.gridgroup.patternview),
    hzoomout = editcommands.build(editcommands.HZOOMOUT, that.gridgroup.patternview),
    vzoomfit = editcommands.build(editcommands.VZOOMFIT, that.gridgroup.patternview),
    vzoomin = editcommands.build(editcommands.VZOOMIN, that.gridgroup.patternview),
    vzoomout = editcommands.build(editcommands.VZOOMOUT, that.gridgroup.patternview),
    zoomfit = editcommands.build(editcommands.ZOOMFIT, that.gridgroup.patternview),
    undo = editcommands.build(editcommands.UNDO, that.sequence, that.cursor), 
    redo = editcommands.build(editcommands.REDO, that.sequence, that.cursor)
  } 
  self.gridgroup.patternview:setcommands(self.editcommands_)
  self.editicons.edit:setcommand(self.editcommands_.editmode)
  self.editicons.select:setcommand(self.editcommands_.selectmode)
  self.editicons.slice:setcommand(self.editcommands_.slicemode)
  self.undoicons.undo:setcommand(self.editcommands_.undo) 
  self.undoicons.redo:setcommand(self.editcommands_.redo) 
  self.dragicons.copy:setcommand(self.editcommands_.copymode)
  self.dragicons.move:setcommand(self.editcommands_.movemode) 
  self.trackgroup.trackicons.itrackfilter:setcommand(self.editcommands_.trackviewmode)
  self.trackgroup.trackicons.itrackeditmode:setcommand(self.editcommands_.trackeditmode)
  self.redrawnotescommand = redrawcommand:new(self.gridgroup.patternview)
  self.redrawnotesandcontrolscommand = redrawgridcommand:new(self.gridgroup,
      redrawgridcommand.NOTES + redrawgridcommand.CONTROLS)
  self.redrawtrackview = redrawcommand:new(self.trackgroup.trackview,
      self.trackgroup.trackheaderview)       
  self.selectioncommands_= {
    transpose1 = multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE,
                                                          that.sequence,
                                                          1),
                                  self.redrawnotescommand),
    transposeM1 = multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE,
                                                           that.sequence,
                                                           -1),
                                   self.redrawnotescommand),
    transpose12 = multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE,
                                                           that.sequence,
                                                           12),
                                   self.redrawnotescommand),
    transposeM12 = multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE,
                                                            that.sequence,
                                                            -12),
                                    self.redrawnotescommand),
    interpolatelinear = multicommand:new(selectioncommands.build(selectioncommands.INTERPOLATELINEAR,
                                                                 that.sequence,
                                                                 that.cursor),
                                         self.redrawnotesandcontrolscommand),    
    interpolatecurve = multicommand:new(selectioncommands.build(selectioncommands.INTERPOLATECURVE,
                                                                that.sequence,
                                                                that.cursor),
                                        self.redrawnotesandcontrolscommand),
    move = multicommand:new(selectioncommands.build(selectioncommands.MOVE,
                                                    that.sequence,
                                                    that.cursor),
                            self.redrawnotesandcontrolscommand),    
    paste = multicommand:new(selectioncommands.build(selectioncommands.PASTE,
                                                    that.sequence,
                                                    that.cursor),
                            self.redrawnotesandcontrolscommand),
    erase = multicommand:new(selectioncommands.build(selectioncommands.ERASE,
                                                    that.sequence),
                             self.redrawnotesandcontrolscommand),
    changetrack = multicommand:new(selectioncommands.build(selectioncommands.CHANGETRACK,
                                                           that.sequence,
                                                           that.cursor),
                                   self.redrawtrackview,
                                   self.redrawnotesandcontrolscommand),
    changegenerator = multicommand:new(selectioncommands.build(selectioncommands.CHANGEGENERATOR,
                                                               that.sequence,
                                                               that.cursor),
                                       self.redrawtrackview,
                                       self.redrawnotesandcontrolscommand),
    changeinstrument = multicommand:new(selectioncommands.build(selectioncommands.CHANGEINSTRUMENT,
                                                                that.sequence,
                                                                that.cursor),
                                        self.redrawtrackview,
                                        self.redrawnotesandcontrolscommand)                                       
  }
  self.selectioncommands_.changeinstrument:execute()
  self.gridgroup.hzoominicon:setcommand(self.editcommands_.hzoomin) 
  self.gridgroup.hzoomouticon:setcommand(self.editcommands_.hzoomout)
  self.gridgroup.hzoomfiticon:setcommand(self.editcommands_.hzoomfit)
  self.gridgroup.vzoominicon:setcommand(self.editcommands_.vzoomin) 
  self.gridgroup.vzoomouticon:setcommand(self.editcommands_.vzoomout)
  self.gridgroup.vzoomfiticon:setcommand(self.editcommands_.vzoomfit)
  self.gridgroup.zoomfiticon:setcommand(self.editcommands_.zoomfit)
  self.gridgroup.keyboardmodeselect:setcommand(self.editcommands_.keyboardmode)
  gridview.editcommands = self:editcommands()
end

function mainviewport:onstatus(text)
  self.statustext:settext(text)
end

function mainviewport:editcommands()
  return self.editcommands_
end

function mainviewport:selectioncommands()
  return self.selectioncommands_
end

function mainviewport:inithostlistener()
  self.sequencebar = sequencebar:new()
  self.hostlistener = hostlistener:new()
  psycle.proxy:addhostlistener(self.hostlistener)
  local that = self
  function self.hostlistener:onsongnew()
    that.cursor:reset()
    that.sequence:update()
    that.scroller:updatehscrollrange(that.gridgroup.patternview)
    that:fls()
  end
  function self.hostlistener:onseqsel() 
    that.cursor:setseqpos(that.sequencebar:editposition() + 1)
    if that.sequencebar:editposition() + 1 > that.sequence:len() then
      that.sequence:update()
    end      
    that.gridgroup.gridpositions:update()  
    that.gridgroup:updatedisplayrange()    
    that.trackgroup.miniview:onsequenceupdated()
    that.scroller:updatehscrollrange(that.gridgroup.patternview)
                 :scrolltoseqpos(that.sequencebar:editposition() + 1)        
  end
  function self.hostlistener:onpatternlength()    
    that.gridgroup.gridpositions:update()  
    that.gridgroup:updatedisplayrange()    
    that.trackgroup.miniview:onsequenceupdated()
    that.scroller:updatehscrollrange(that.gridgroup.patternview)
                 :scrolltoseqpos(that.sequencebar:editposition() + 1)
    --that.gridgroup.patternview:createbackground()                 
    that.gridgroup.patternview:fls()                 
  end
  function self.hostlistener:onundopattern()
    that.sequence:update()
  end
  function self.hostlistener:onpatkeyup()
    that.sequence:updateseqpos(that.sequencebar:editposition() + 1)
    that.gridgroup.patternview:fls()
    that.gridgroup:flscontrolview()
  end
  function self.hostlistener:onsongloaded()
    that.cursor:reset()
    that.sequence:update()
    that.scroller:updatehscrollrange(that.gridgroup.patternview)
    that:fls()
  end
  function self.hostlistener:ontracknumchanged()
    that.cursor:checktracknum()
    that.cursor:notify()
  end
 -- function self.hostlistener:onticksperbeatchanged()
  --  that.sequence:update()                
  --  that.scroller:updatehscrollrange(that.gridgroup.patternview)
 --              :scrolltoseqpos(that.sequencebar:editposition() + 1)
 -- end
  function self.hostlistener:onpattern()
    that.sequence:update()
  end
  function self.hostlistener:onseqmodified()
    that.sequence:update() 
  end
  function self.hostlistener:onoctaveup()
    that.gridgroup.keyboard:createkeys():fls()     
  end
  function self.hostlistener:onoctavedown()
    that.gridgroup.keyboard:createkeys():fls()
  end
end

function mainviewport:toggleseqdisplay()
  self.gridgroup:toggleseqdisplay()  
  self.trackgroup.miniview:toggleseqdisplay()
  self.scroller:updatehscrollrange()
               :scrolltoseqpos(self.sequencebar:editposition() + 1 )   
end

function mainviewport:initcontrolscroll(parent)
  scrollbar:new(parent)
           :setalign(alignstyle.RIGHT)
           :setautoize(false, false)
           :setdimension(dimension:new(20, 0))
end

function mainviewport:onmousedown(ev)
  self:setfocus()
end

function mainviewport:onwheel(ev)
  if ev:ctrlkey() then
    if ev:wheeldelta() < 0 then
      self.editcommands_.hzoomin:execute()
    elseif ev:wheeldelta() > 0 then
      self.editcommands_.hzoomout:execute()
    end
  else
    self.scroller:onwheel(ev)
  end
end

function mainviewport:onkeydown(ev)
  self.trackgroup.trackview:handlekeyinput(ev, self.gridgroup.patternview)
  if not ev:ispropagationstopped() then
    self.inputhandler:handlekeyinput(ev)
    if not ev:ispropagationstopped() then
      self.gridgroup.patternview.statehandler:transition("handlekeydown", self.gridgroup.patternview, ev)
      self.gridgroup.controlgroup.controlview.statehandler:transition("handlekeydown", self.gridgroup.patternview, ev)
    end
  end      
end

function mainviewport:initedittoolbar(parent)  
  local t = toolbar:new(parent)
                   :setalign(alignstyle.LEFT)
                   :setautosize(true, false)
                   :setgroupindex(1)
  self.editicons = {   
    edit = toolicon:new(t, icondir .. "pen.png", transparency)
                   :enabletoggle()
                   :seton(true),
    select = toolicon:new(t, icondir .. "select.png", transparency)
                     :enabletoggle(),
    slice = toolicon:new(t, icondir .. "slice.png", transparency)
                    :enabletoggle(),    
  }
  for _, icon in pairs(self.editicons) do    
    self:formattoolicon(icon)
    icon:setgroupindex(1)
  end
  self.undoicons = {
    undo = toolicon:new(t, icondir .. "undo.png", transparency),
    redo = toolicon:new(t, icondir .. "redo.png", transparency)
  }
  for _, icon in pairs(self.undoicons) do    
    self:formattoolicon(icon)
  end
end

function mainviewport:initdragtoolbar(parent)  
  local t = toolbar:new(parent)
                   :setalign(alignstyle.LEFT)
                   :setautosize(true, false)
                   :setgroupindex(2)
  self.dragicons = {  
    move = toolicon:new(t, icondir .. "move.png", transparency),
    copy = toolicon:new(t, icondir .. "copy.png", transparency)        
  }
  for _, icon in pairs(self.dragicons) do    
    self:formattoolicon(icon)
    icon:enabletoggle():setgroupindex(2)
  end 
  self.dragicons.move:seton(true)
  self.dragtoolbar = t
end

function mainviewport:initoptiontoolbar(parent)  
  local t = toolbar:new(parent)
                   :setalign(alignstyle.LEFT)
                   :setautosize(true, false)
                   :setgroupindex(2)
  self.optionicons = {  
    chords = toolicon:new(t, icondir .. "chords.png", transparency),         
  }
  for _, icon in pairs(self.optionicons) do    
    self:formattoolicon(icon)
  end
  self.optionicons.chords:settext("Insert Note")
  self.optionicons.chords:enabletoggle()
  self.optionicons.chords:setdimension(dimension:new(100, 0))
  self.optionicons.chords:setjustify(toolicon.LEFTJUSTIFY)
  local that = self
  function self.optionicons.chords:onclick()
    if that.chordselector:visible() then
      that.chordselector:hide()
    else
      that.chordselector:show()
    end
    that:updatealign()
  end
  self.optiontoolbar = t
end

function mainviewport:oninsertchordchanged(chordselector)
  self.optionicons.chords:settext("Insert " .. chordselector:chord():name())
  self.gridgroup.patternview:setchord(chordselector:chord())
end
 
function mainviewport:formattoolicon(icon)    
  icon:setautosize(false, false)
      :setverticalalignment(toolicon.CENTER)
      :setjustify(toolicon.CENTERJUSTIFY)
      :setdimension(dimension:new(20, 0))
  return self
end

return mainviewport
