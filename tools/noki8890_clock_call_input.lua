-- Outgoing call from physically provisioned clock/date and ordinary idle.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_outgoing_call = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
