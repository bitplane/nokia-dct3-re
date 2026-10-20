-- Correct cold SIM PIN, then the distinct phone security code.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine = manager.machine
local contacts = {{2,2},{3,2},{4,2},{2,4},{3,4},{1,4}}
local events = {}
local pin = _G.nsk1_replacement_pin and {4,3,2,1,6} or {1,2,3,4,6}
for _,phase in ipairs({{12,'pin',pin},{_G.nsk1_phone_input_time or 18,'phone',{1,2,3,4,5,6}}}) do
    for index,key in ipairs(phase[3]) do
        local field
        for _,candidate in pairs(machine.ioport.ports[':COL.'..contacts[key][1]].fields) do
            if candidate.mask == contacts[key][2] then field = candidate end
        end
        assert(field,'own security contact missing')
        events[#events+1] = {phase[1]+(index-1)*0.5,field,1,index,phase[2]}
        events[#events+1] = {phase[1]+(index-1)*0.5+0.2,field,0,index,phase[2]}
    end
end
local next_event,next_frame = 1,1
local times = {10,15,17,22,25,30}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk1_pin_contact: phase=%s index=%d down=%d t=%.9f physical_only=1\n',event[5],event[4],event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #times and now >= times[next_frame] then
        machine.screens[':screen']:snapshot('nsk1_pin_reload_'..next_frame..'.png')
        next_frame = next_frame+1
    end
end,'NSK-1 cold SIM and phone security')
