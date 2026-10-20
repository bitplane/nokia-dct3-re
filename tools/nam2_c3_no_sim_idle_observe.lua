-- Physical card absence; no early security transaction or firmware writes.
local machine = manager.machine
assert(machine.system.name == 'nam2c3')
machine.ioport.ports[':SIM_REMOVED'].fields['Remove SIM card']:set_value(1)
machine:logerror('nam2_idle_card_control: removed=1 firmware_writes=0\n')
_G.nam2_c3_idle_no_early_keys = true
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'nam2_c3_idle_observe.lua')
