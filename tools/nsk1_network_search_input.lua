-- Own English NSK-1 menu traversal; physical contacts only, not driver policy.
_G.nsk1_radio_no_call = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_radio_observe.lua')
local machine, keys = manager.machine, {}
for name, cell in pairs({menu={1,4}, down={0,8}}) do
    for _, field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name] = field end
    end
    assert(keys[name], 'own network-menu contact missing')
end
-- Main Settings -> Phone settings -> Network selection -> Manual.
local sequence = {'menu','down','down','down','menu','menu','down',
                  'menu','down','down','down','menu','down','menu'}
local events, frames = {}, {}
for index, key in ipairs(sequence) do
    local time = 21 + index * 2
    events[#events+1] = {time,key,1,index}
    events[#events+1] = {time+0.2,key,0,index}
    frames[#frames+1] = time+0.5
end
local event, frame = 1,1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while events[event] and now >= events[event][1] do
        local step = events[event]
        keys[step[2]]:set_value(step[3])
        machine:logerror(string.format('nsk1_network_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',
            step[4],step[2],step[3],now))
        event=event+1
    end
    if frames[frame] and now >= frames[frame] then
        machine.screens[':screen']:snapshot('nsk1_network_'..frame..'.png')
        frame=frame+1
    end
end, 'NSK-1 physical manual network search')
