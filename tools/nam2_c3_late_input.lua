-- NAM-2 v5.21 late physical input, after the idle loop's clock stops.
-- Early security input as in the replay; at 20 s, with the set-time prompt
-- up and the idle loop long past its first deadline decline, press 1 then 2.
-- The prompt must show them (snapshots late_before/late_after).
local machine = manager.machine
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki2100_v521_early_security_input.lua')
local function field(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('no input field ' .. name)
end
local late = coroutine.create(function()
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('late_before.png')
    for _, name in ipairs({'Keypad 1', 'Keypad 2'}) do
        local key = field(name)
        machine:logerror(string.format('nam2_late_input: key=%s t=%.3f\n', name, machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.2) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.6) then return end
    end
    if not emu.wait(1) then return end
    machine.screens[':screen']:snapshot('late_after.png')
end)
_G.nam2_c3_late_input = late
assert(coroutine.resume(late))
