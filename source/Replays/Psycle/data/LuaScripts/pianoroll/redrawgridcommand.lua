local redrawgridcommand = {}

redrawgridcommand.NOTES = 1
redrawgridcommand.CONTROLS = 2
redrawgridcommand.KEYBOARD = 4
redrawgridcommand.CONTROLBOARD = 8
redrawgridcommand.ALL = 16

function redrawgridcommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function redrawgridcommand:init(gridgroup, mode)
  self.gridgroup_ = gridgroup
  self.mode_ = mode and mode or redrawgridcommand.ALL
end

function redrawgridcommand:execute()
  if self.mode_ == redrawgridcommand.ALL then
    self.gridgroup_:fls()
  elseif self.mode_ == redrawgridcommand.NOTES then
    self.gridgroup_.patternview:fls()
  elseif self.mode_ == redrawgridcommand.CONTROLS then
    self.gridgroup_.controlgroup.controlview:fls()
  elseif self.mode_ == (redrawgridcommand.CONTROLS + redrawgridcommand.NOTES) then
    self.gridgroup_.controlgroup.controlview:fls()
    self.gridgroup_.patternview:fls()
  end
end

return redrawgridcommand
