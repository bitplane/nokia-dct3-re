-- Restore only after the physical Snooze has powered this handset off.
_G.noki6210_alarm_snooze = true
_G.noki6210_alarm_restore_snooze = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_power_restore.lua')
