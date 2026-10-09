-- Physical default-code attempt; no editor or EEPROM state is modified.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'nse6_stage_observe.lua')
local machine = manager.machine
local keys = {
    {8, ':COL.2', 'Keypad 1'},
    {9, ':COL.3', 'Keypad 2'},
    {10, ':COL.4', 'Keypad 3'},
    {11, ':COL.2', 'Keypad 4'},
    {12, ':COL.3', 'Keypad 5'},
    {13, ':COL.1', 'Left Softkey'},
}
local next_key, held = 1, nil
local before_submit, after_submit = false, false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if held and now >= held[1] + 0.2 then
        machine.ioport.ports[held[2]].fields[held[3]]:set_value(0)
        held = nil
    end
    if not held and next_key <= #keys and now >= keys[next_key][1] then
        held = keys[next_key]
        assert(machine.ioport.ports[held[2]].fields[held[3]]):set_value(1)
        machine:logerror(string.format(
            'nse6_security_contact: label=%s t=%.9f\n', held[3], now))
        next_key = next_key + 1
    end
    if now >= 12.5 and not before_submit then
        before_submit = true
        machine.screens[':screen']:snapshot('8810-security-entered.png')
    end
    if now >= 13.5 and not after_submit then
        after_submit = true
        machine.screens[':screen']:snapshot('8810-security-result.png')
    end
end, 'frame')
