-- Cold Outbox navigation using only physical contacts and saved card input.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_security_fixture.lua')
local machine=manager.machine
local function contact(column,mask)
    for _,field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask==mask then return field end
    end
    error('NSE-2 Outbox contact missing')
end
local events={}
local presses={{20,1,4},{20.5,0,8},{21,1,4},{21.5,0,8},{22,1,4}}
if _G.nse2_sms_action~='empty' then presses[#presses+1]={23.5,1,4} end
for _,press in ipairs(presses) do
    local field=contact(press[2],press[3])
    events[#events+1]={press[1],field,1}
    events[#events+1]={press[1]+0.15,field,0}
end
if _G.nse2_sms_action and _G.nse2_sms_action~='empty' then
    for _,press in ipairs({{24.3,1,4},{24.8,1,4},
                           {25.5,1,_G.nse2_sms_action=='cancel' and 2 or 4}}) do
        local field=contact(press[2],press[3])
        events[#events+1]={press[1],field,1}
        events[#events+1]={press[1]+0.15,field,0}
    end
end
local next_event,next_frame=1,1
local frames={21.8,22.3,23.3,25,26.5,29}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while next_event<=#events and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nse2_sms_reload_contact: event=%d down=%d t=%.9f physical_only=1\n',
            next_event,event[3],now))
        next_event=next_event+1
    end
    if next_frame<=#frames and now>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nse2_sms_reload_' .. next_frame .. '.png')
        next_frame=next_frame+1
    end
end,'NSE-2 physical cold Outbox read')
