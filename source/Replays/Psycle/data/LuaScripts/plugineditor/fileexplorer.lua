-- psycle fileexplorer (c) 2016 by psycledelics
-- File: fileexplorer.lua
-- copyright 2016 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local signal = require("psycle.signal")
local node = require("psycle.node")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local image = require("psycle.ui.image")
local images = require("psycle.ui.images")
local alignstyle = require("psycle.ui.alignstyle")
local group = require("psycle.ui.group")
local edit = require("psycle.ui.edit")
local listview = require("psycle.ui.listview")
local text = require("psycle.ui.text")
local filehelper = require("psycle.file")
local closebutton = require("psycle.ui.closebutton")
local toolicon = require("psycle.ui.toolicon")

local fileexplorer = group:new()

function fileexplorer:new(parent)  
  local m = group:new(parent)  
  setmetatable(m, self)
  self.__index = self
  m:init()
  return m
end

function fileexplorer:init()
   self:initlistview()   
   self:initicons()
   self:initheader() 
   self:initsignals()
end

function fileexplorer:initheader()  
  self.header = group:new(self)
                     :setalign(alignstyle.TOP)
                     :setautosize(false, false)
					           :setposition(rect:new(point:new(0, 0), dimension:new(0, 25)))
					           :addornament(ornamentfactory:createfill(0x333333))
  -- local machinehomeicon = toolicon:new(self.header, psycle.setting():dir("icons") .. "home.png", 0xFFFFFF)                          
  --machinehomeicon:setautosize(false, false)
  -- machinehomeicon:setverticalalignment(toolicon.CENTER)
  --machinehomeicon:setjustify(toolicon.CENTERJUSTIFY)
--  machinehomeicon:setposition(rect:new(point:new(), dimension:new(200, 0)))
  --machinehomeicon:setalign(alignstyle.CLIENT)  
  --machinehomeicon:settext("Machine Home")
  --machinehomeicon.cc = 0x333333
  local on = image:new():load(psycle.setting():dir("icons") .. "poweron.png")
  on:settransparent(0xFFFFFF)					 
  local closebutton = closebutton.new(self.header):settext("<<")
  local that = self
  function closebutton:onclick()
    that:setposition(rect:new(point:new(0, 0), dimension:new(0, 0)))    
	  that:parent():flagnotaligned():updatealign()     
  end  
end

function fileexplorer:initlistview()
  self.homepath = psycle.setting():dir("lua")
  self.currentpath = self.homepath
  self.listview = listview:new(self)
                          :setalign(alignstyle.CLIENT)
						              :setautosize(false, false)
						              :viewlist()			
						              :setmargin(boxspace:new(0, 0, 0, 10))
  local that = self
  function self.listview:ondblclick(ev)    
	local node = self:selected()
	if node and not node.isdirectory then  
	  local ev = { 
		sender = self, 
		path = node.path,
		filename = node.filename,
		extension = node.extension
	  }
	  that.click:emit(ev)
	elseif node then
	  that:setpath(node.path.."\\"..node.filename)
	end
  end
end

function fileexplorer:initicons()
  local icondir = psycle.setting():dir("icons")
  self.text = image:new():load(icondir .. "document.png")
  self.textlua = image:new():load(icondir .. "document.png")
  self.folder = image:new():load(icondir .. "folder_small.png")
  self.folder = image:new():load(icondir .. "folder_small.png")  
  self.home = image:new():load(icondir .. "home.png")
  self.images = images:new()
  self.images:add(self.text)
             :add(self.folder)
			       :add(self.textlua)
			       :add(self.home)   
  self.listview:viewsmallicon()
  self.listview:setimages(self.images)  
end

function fileexplorer:initsignals()
  self.click = signal:new()
  self.onremove = signal:new()
  self.onselectmachine = signal:new()
  self.doedit = signal:new()
  self.dohide = signal:new()  
  self.doreturn = signal:new()  
end

function fileexplorer:sethomepath(path)
  self.homepath = path
  self:setpath(path)
end

function fileexplorer:setpath(path)    
  self.currentpath = path
  local dir = filehelper.directorylist(path)  
  self.rootnode = node:new()
  --self:addhomenode()
  self:addparentdirectorynode(path)
  for i=1, #dir do
    if dir[i] then      
      local node = node:new():settext(dir[i].filename)	  
	    node.filename = dir[i].filename
	    node.isdirectory = dir[i].isdirectory
	    node.extension = dir[i].extension
	    node.path = dir[i].path	  
	    if node.isdirectory then	    
        node:setimageindex(1):setselectedimageindex(1)
      elseif node.extension == ".lua" then
        node:setimageindex(2):setselectedimageindex(2)
      end	  
	    self.rootnode:add(node)   
	  end
  end    
  self.listview:setrootnode(self.rootnode) 
  self.listview:updatelist()
end

function fileexplorer:path()  
  return self.currentpath
end

function fileexplorer:addhomenode()
  local dir = filehelper.fileinfo(self.homepath)
  local node = node:new():settext("Machine Home")
  node:setimageindex(3):setselectedimageindex(3)
  node.isdirectory = true
  node.filename = dir.filename
  node.extension = ""  
  node.path = dir.path	
  self.rootnode:add(node)  
end

function fileexplorer:addparentdirectorynode(path)
  local parentdirectory = filehelper.parentdirectory(path)
  if parentdirectory then
	local node = node:new():settext(".. ["..parentdirectory.filename.."]")
	node:setimageindex(1):setselectedimageindex(1)
	node.isdirectory = true
	node.filename = parentdirectory.filename
	node.extension = ""  
	node.path = parentdirectory.path	
	self.rootnode:add(node)  
  end
end

function fileexplorer:update()
  self:setpath(self.currentpath)
end

return fileexplorer
