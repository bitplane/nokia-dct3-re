_G.nse2_phone_code_start=9
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_security_fixture.lua')
local machine=manager.machine
local mapping={['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},['5']={3,4},['6']={4,4},['7']={2,8},['8']={3,8},['M']={1,4},['D']={0,8}}
local events={}
local function press(t,key)
    local cell=mapping[key]
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask==cell[2] then
            events[#events+1]={t,field,1}
            events[#events+1]={t+.15,field,0}
            return
        end
    end
    error('PUK2 physical contact missing')
end
local path='MDDDMDDMDDDDMDDM'
for i=1,#path do press(15.5+(i-1)*.35,path:sub(i,i)) end
local credentials=(_G.nse2_puk2_wrong and '87654325' or '87654321')..'M2468M2468M'
for i=1,#credentials do press(24.5+(i-1)*.25,credentials:sub(i,i)) end
local event,frame=1,1
local frames={14,24.3,26.65,27.9,29.7}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local e=events[event]
        e[2]:set_value(e[3])
        machine:logerror(string.format('nse2_puk2_contact: event=%d down=%d t=%.9f physical_only=1\n',event,e[3],now))
        event=event+1
    end
    if frame<=#frames and now>=frames[frame] then
        machine.screens[':screen']:snapshot('nse2_puk2_'..frame..'.png')
        frame=frame+1
    end
end,'NSE-2 physical PUK2 probe')
