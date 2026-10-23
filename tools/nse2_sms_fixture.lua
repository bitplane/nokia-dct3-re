-- Physical Messages / Write / Save probe; no message or card bytes injected.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_security_fixture.lua')
local machine=manager.machine
local function contact(column,mask)
    for _,field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask==mask then return field end
    end
    error('NSE-2 SMS contact missing')
end
local events={}
for _,press in ipairs({{20,1,4},{20.5,0,8},{21,1,4},{21.5,0,8},{22,0,8},
                       {22.5,1,4},{23,3,2},{24.3,1,4},{24.8,0,8},{25.3,0,8},{25.8,1,4}}) do
    local field=contact(press[2],press[3])
    events[#events+1]={press[1],field,1}
    local hold=press[1]==25.8 and _G.nse2_sms_save_hold or .15
    assert(hold>0 and hold<=.15)
    events[#events+1]={press[1]+hold,field,0}
end
local next_event,next_frame=1,1
local frames={20.8,21.3,22.3,22.8,24,24.6,25.6,26.5,29}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while next_event<=#events and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nse2_sms_contact: event=%d down=%d t=%.9f physical_only=1\n',
            next_event,event[3],now))
        next_event=next_event+1
    end
    if next_frame<=#frames and now>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nse2_sms_' .. next_frame .. '.png')
        next_frame=next_frame+1
    end
end,'NSE-2 physical offline SMS storage probe')
