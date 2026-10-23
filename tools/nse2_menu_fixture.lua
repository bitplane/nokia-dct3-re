-- Physical Menu contacts only; the accompanying observer never writes guest state.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_boot_observe.lua')
local machine = manager.machine
assert(machine.system.name == 'nse2r4t' or machine.system.name == 'nse2r4v')
local menu
for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
    if field.mask == 4 then menu = field end
end
assert(menu, 'NSE-2 physical Menu contact missing')
local events = {{3, 1}, {3.2, 0}, {5, 1}, {5.2, 0}}
local next_event, next_frame = 1, 1
local frames = {4, 6}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        menu:set_value(event[2])
        machine:logerror(string.format('nse2_menu_contact: index=%d down=%d t=%.9f physical_only=1\n',
            (next_event + 1) // 2, event[2], now))
        next_event = next_event + 1
    end
    if next_frame <= #frames and now >= frames[next_frame] then
        machine.screens[':screen']:snapshot('nse2_menu_' .. next_frame .. '.png')
        next_frame = next_frame + 1
    end
end, 'NSE-2 physical Menu probe')
