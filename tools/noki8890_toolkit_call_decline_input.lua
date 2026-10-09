-- Physical refusal, without firmware state or task-message intervention.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8890_toolkit_call_decline = true
dofile(directory .. 'noki8890_toolkit_call_input.lua')
