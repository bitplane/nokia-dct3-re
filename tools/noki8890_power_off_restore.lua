-- Emulator state restoration only; physical input remains in the shared fixture.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_power_restore_off = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_power_cycle_observe.lua')
