-- Physical right-softkey Snooze; the firmware supplies its next deadline.
_G.noki8890_alarm_snooze = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_alarm_input.lua')
