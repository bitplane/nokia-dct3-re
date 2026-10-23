-- Physical SIM PIN entry only; the phone-side lock code is untouched.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. (_G.nse2_pin_phone_auth and 'nse2_security_fixture.lua' or 'nse2_boot_observe.lua'))
local machine=manager.machine
local mapping={['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},['6']={4,4},['8']={3,8}}
local events={}
local function press(when,column,mask)
    for _,field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask==mask then
            events[#events+1]={when,field,1}
            events[#events+1]={when+0.15,field,0}
            return
        end
    end
    error('NSE-2 PIN contact missing')
end
for index=1,4 do
    local key=assert(mapping[_G.nse2_pin_value:sub(index,index)])
    press(6+(index-1)*0.35,key[1],key[2])
end
press(7.6,1,4)
local next_event,next_frame=1,1
local frames={5.5,9,11}
if _G.nse2_pin_replay_on_load then
    _G.nse2_pin_load_notifier=emu.add_machine_post_load_notifier(function()
        local now=machine.time:as_double()
        local levels={}
        next_event=1
        while next_event<=#events and events[next_event][1]<=now do
            local event=events[next_event]
            levels[event[2]]=event[3]
            next_event=next_event+1
        end
        -- Forced physical contacts are harness state, not handset save items.
        for field,value in pairs(levels) do field:set_value(value) end
        machine:logerror('nse2_pin_contact_replay: restored=1 physical_only=1\n')
    end)
end
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while next_event<=#events and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nse2_pin_contact: event=%d down=%d t=%.9f physical_only=1\n',next_event,event[3],now))
        next_event=next_event+1
    end
    if next_frame<=#frames and now>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nse2_pin_' .. next_frame .. '.png')
        next_frame=next_frame+1
    end
end,'NSE-2 physical SIM PIN entry')
