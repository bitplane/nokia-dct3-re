-- Continue the own-ROM security fixture using physical menu/navigation keys.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'nse6_security_fixture.lua')
local machine = manager.machine
local sequence = {
    {18, ':COL.1', 'Left Softkey'},
    {20, ':COL.1', 'Down'},
    {22, ':COL.1', 'Left Softkey'},
}
local next_key, held = 1, nil
local snapshots = {19, 21, 23}
local next_snapshot = 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if held and now >= held[1] + 0.2 then
        machine.ioport.ports[held[2]].fields[held[3]]:set_value(0)
        held = nil
    end
    if not held and next_key <= #sequence and now >= sequence[next_key][1] then
        held = sequence[next_key]
        assert(machine.ioport.ports[held[2]].fields[held[3]]):set_value(1)
        machine:logerror(string.format(
            'nse6_menu_contact: label=%s t=%.9f\n', held[3], now))
        next_key = next_key + 1
    end
    if next_snapshot <= #snapshots and now >= snapshots[next_snapshot] then
        machine.screens[':screen']:snapshot('8810-menu-' .. next_snapshot .. '.png')
        next_snapshot = next_snapshot + 1
    end
end, 'frame')
