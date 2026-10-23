-- Search a contact on a card saved by a separate cold process.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
local machine = manager.machine
local function contact(column, mask)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == mask then return field end
    end
    error('NSE-2 phonebook readback contact missing')
end
local events = {}
for _, press in ipairs({{23.5,1,4},{24,3,2},{25.3,1,4}}) do
    local field = contact(press[2],press[3])
    events[#events+1] = {press[1],field,1}
    events[#events+1] = {press[1]+0.15,field,0}
end
local next_event, next_frame = 1, 1
local frames = {24.8,26,28,30}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nse2_phonebook_reload_contact: event=%d down=%d t=%.9f physical_only=1\n',
            next_event,event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #frames and now >= frames[next_frame] then
        machine.screens[':screen']:snapshot('nse2_phonebook_reload_' .. next_frame .. '.png')
        next_frame = next_frame+1
    end
end,'NSE-2 physical cold contact search')
