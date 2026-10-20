-- Physical cancellation after a bounded manual-search observation.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_network_search_input.lua')
local machine, menu = manager.machine, nil
for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
    if field.mask == 4 then menu = field end
end
assert(menu, 'own physical Quit contact missing')
local times, frame, pressed, released = {80,89,92,99},1,false,false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now >= 90 and not pressed then
        pressed=true
        menu:set_value(1)
        machine:logerror(string.format('nsk1_network_quit: down=1 t=%.9f physical_only=1\n',now))
    end
    if now >= 90.2 and not released then
        released=true
        menu:set_value(0)
        machine:logerror(string.format('nsk1_network_quit: down=0 t=%.9f physical_only=1\n',now))
    end
    if times[frame] and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_network_late_'..frame..'.png')
        machine:logerror(string.format('nsk1_network_late: t=%.9f mcu_pc=%08x dsp_pc=%04x observer_writes=0\n',
            now,machine.devices[':maincpu'].state['PC'].value,machine.devices[':dsp_c54x:cpu'].state['PC'].value))
        frame=frame+1
    end
end, 'NSK-1 physical search cancellation')
