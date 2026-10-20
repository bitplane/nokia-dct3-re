-- Read-only startup observation plus six physical keypad contacts.
assert(manager.machine.system.name == 'nam2c3')
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki2100_v521_startup_observe.lua')
dofile(directory .. 'noki2100_v521_early_security_input.lua')
