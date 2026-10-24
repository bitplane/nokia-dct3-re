_G.nsb1_diagnostic_display = true
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb1_idle_input.lua')
