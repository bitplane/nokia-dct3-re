-- Only physical contacts; the imported card must come from real exhaustion.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_boot_observe.lua')
local machine=manager.machine
local mapping={['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},['5']={3,4},['6']={4,4},['7']={2,8},['8']={3,8}}
local events={}
local function press(t,col,mask)
    for _,field in pairs(machine.ioport.ports[':COL.'..col].fields) do
        if field.mask==mask then
            events[#events+1]={t,field,1,col..':'..mask}
            events[#events+1]={t+0.15,field,0,col..':'..mask}
            return
        end
    end
    error('missing physical contact')
end
local function digits(t,value,step)
    for i=1,#value do
        local key=mapping[value:sub(i,i)]
        press(t+(i-1)*step,key[1],key[2])
    end
end
digits(11.2,_G.nse2_wrong_puk1 and '12345675' or '12345678',0.30)
press(13.8,1,4)
digits(14.4,'2468',0.35)
press(15.9,1,4)
digits(16.6,'2468',0.35)
press(18.2,1,4)
local frames={11,14.1,16.3,20,25}
local event,frame=1,1
if _G.nse2_puk_replay_on_load then
    _G.nse2_puk_load_notifier=dofile(directory..'restore_contact_timeline.lua')(
        events,function(index) event=index end,'nse2_puk_contact_replay')
end
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local e=events[event]
        e[2]:set_value(e[3])
        machine:logerror(string.format('nse2_puk_contact: event=%d down=%d t=%.9f physical_only=1\n',event,e[3],now))
        event=event+1
    end
    if frame<=#frames and now>=frames[frame] then
        machine.screens[':screen']:snapshot('nse2_puk_probe_'..frame..'.png')
        frame=frame+1
    end
end,'NSE-2 physical PUK recovery probe')
