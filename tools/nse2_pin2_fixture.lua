-- Menu layout belongs to this physical-input fixture, not the driver.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_security_fixture.lua')
local machine=manager.machine
local mapping={['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},['5']={3,4},['6']={4,4},['7']={2,8},['8']={3,8},['9']={4,8},['0']={3,16},['M']={1,4},['D']={0,8}}
local old,new='5678','2468'
if _G.nse2_pin2_restore then old,new=new,old end
if _G.nse2_pin2_wrong then old='5675' end
local sequence='MDDDMDDMDDDDMDDM'..(_G.nse2_pin2_prompt_only and '' or old..'M'..new..'M'..new..'M')
local events={}
for i=1,#sequence do
    local key=mapping[sequence:sub(i,i)]
    for _,field in pairs(machine.ioport.ports[':COL.'..key[1]].fields) do
        if field.mask==key[2] then
            events[#events+1]={18.5+(i-1)*.35,field,1}
            events[#events+1]={18.65+(i-1)*.35,field,0}
        end
    end
end
local event,frame=1,1
local frames={18,23.95,25.7,27.45,29.7}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local e=events[event]
        e[2]:set_value(e[3])
        machine:logerror(string.format('nse2_pin2_contact: event=%d down=%d t=%.9f physical_only=1\n',event,e[3],now))
        event=event+1
    end
    if frame<=#frames and now>=frames[frame] then
        machine.screens[':screen']:snapshot('nse2_pin2_menu_'..frame..'.png')
        frame=frame+1
    end
end,'NSE-2 physical PIN2 security menu probe')
