-- Physical Add name workflow before the known native receive restart.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
local machine = manager.machine
local function contact(column, mask)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == mask then return field end
    end
    error('NSE-2 phonebook contact missing')
end
local events = {}
local function press(when, column, mask)
    local field = contact(column, mask)
    events[#events + 1] = {when, field, 1}
    local hold=when==27.5 and _G.nse2_phonebook_save_hold or 0.15
    assert(hold>0 and hold<=0.15)
    events[#events + 1] = {when + hold, field, 0}
end
press(23.1, 0, 8) -- Add name
press(23.5, 1, 4)
press(24.0, 3, 2) -- A
press(25.3, 1, 4)
press(26.0, 2, 2) -- 123
press(26.35, 3, 2)
press(26.7, 4, 2)
press(27.5, 1, 4)
local next_event, next_frame = 1, 1
local frames = {23.9, 25.1, 25.8, 27.3, 28.3, 30}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nse2_phonebook_contact: event=%d down=%d t=%.9f physical_only=1\n',
            next_event, event[3], now))
        next_event = next_event + 1
    end
    if next_frame <= #frames and now >= frames[next_frame] then
        machine.screens[':screen']:snapshot('nse2_phonebook_' .. next_frame .. '.png')
        next_frame = next_frame + 1
    end
end, 'NSE-2 physical Add name workflow')
