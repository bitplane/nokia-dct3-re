-- Physical midnight fixture plus read-only date-scalar observations.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_midnight_input.lua')
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces['program']
local input = coroutine.create(function()
    if not emu.wait(50) then return end
    machine:logerror(string.format('8890_calendar: stage=before scalar=%08x\n',
        memory:read_u32(0x137420)))
    if not emu.wait(50) then return end
    machine:logerror(string.format('8890_calendar: stage=after scalar=%08x\n',
        memory:read_u32(0x137420)))
end)
_G.noki8890_calendar_rollover_input = input
assert(coroutine.resume(input))
