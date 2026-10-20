-- Physical FDN capability experiment against an explicit extended lab card.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsm1_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{4,2,3},{1,2,0x19},
    {1,4,0x17},{1,2,0x19},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19}}
if _G.nsm1_fdn_disable then
    _G.nsm1_followup_contacts[6]={1,8,0x18}
elseif _G.nsm1_fdn_observe then
    _G.nsm1_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{4,2,3}}
end
if _G.nsm1_fdn_wrong_activation then
    _G.nsm1_followup_contacts[11]={3,4,5}
end
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local received=0
_G.nsm1_fdn_rx_tap=memory:install_read_tap(0x20034,0x20037,'nsm1_fdn_rx',function(_,value,mask)
    if mask & 0xff==0 then return end
    received=received+1
    if received<=4096 then
        machine:logerror(string.format('nsm1_fdn_rx: index=%d value=%02x observer_writes=0\n',received,value&0xff))
    end
end)
local index=1
local times={24,25,26,28,30,32,34}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_fdn_'..times[index]..'.png')
        index=index+1
    end
end)
