-- Physical own-menu FDN input; APDU observations are read-only.
local source=debug.getinfo(1,'S').source:sub(2)
-- Allow extended-card startup to settle before physical phone input.
_G.nsk1_phone_input_time=24
dofile(source:match('^(.*[/\\])') .. 'nsk1_pin_reload.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local decoded=0
_G.nsk1_fdn_key_tap=cpu.spaces.program:install_read_tap(0x294860,0x294863,'nsk1_fdn_key',function()
    if cpu.state['R15'].value~=0x294862 or decoded>=128 then return end
    decoded=decoded+1
    machine:logerror(string.format('nsk1_fdn_key: value=%02x t=%.9f observer_writes=0\n',cpu.state['R0'].value,machine.time:as_double()))
end)
local count=0
_G.nsk1_fdn_tx_tap=machine.devices[':maincpu'].spaces.program:install_write_tap(0x20034,0x20037,'nsk1_fdn_tx',function(_,value,mask)
    if mask & 0xff00==0 then return end
    count=count+1
    if count<=4096 then
        machine:logerror(string.format('nsk1_sim_tx: index=%d value=%02x t=%.9f observer_writes=0\n',count,(value>>8)&0xff,machine.time:as_double()))
    end
end)
local cells={menu={1,4},clear={1,2},down={0,8},up={1,16},one={2,2},two={3,2},three={4,2},four={2,4},five={3,4},six={4,4},seven={2,8},eight={3,8},hash={4,16}}
local keys={'menu','down','down','down','menu','down','down','menu','down','menu'}
if _G.nsk1_fdn_cold then
    table.remove(keys)
else
    for _,key in ipairs({_G.nsk1_fdn_disable and 'down' or 'up','menu','five','six','seven','eight','menu'}) do
        keys[#keys+1]=key
    end
end
keys=_G.nsk1_fdn_keys or keys
local events,frames={},{}
for index,name in ipairs(keys) do
    local cell=cells[name]
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'own FDN contact missing')
    local when=_G.nsk1_fdn_times and _G.nsk1_fdn_times[index] or 32+index*2
    events[#events+1]={when,field,1,index,name}
    local duration=index==#keys and _G.nsk1_fdn_hold_last or 0.2
    events[#events+1]={when+duration,field,0,index,name}
    frames[#frames+1]=when+(_G.nsk1_fdn_frame_delay or 0.5)
end
local event,frame=1,1
local final=false
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[event] and now>=events[event][1] do
        local step=events[event]
        step[2]:set_value(step[3])
        machine:logerror(string.format('nsk1_fdn_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',step[4],step[5],step[3],now))
        event=event+1
    end
    if frames[frame] and now>=frames[frame] then
        machine.screens[':screen']:snapshot('nsk1_fdn_'..frame..'.png')
        frame=frame+1
    end
    if not final and now>=(_G.nsk1_fdn_final_time or (_G.nsk1_fdn_cold and 62 or 74)) then
        final=true
        machine.screens[':screen']:snapshot('nsk1_fdn_final.png')
    end
end)
