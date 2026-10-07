-- Physical invalid-time rejection followed by valid editor recovery.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_invalid_first = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
