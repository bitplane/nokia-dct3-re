-- Protected fixed-number entry/read/rejection using physical contacts only.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_security_fixture.lua')
local machine=manager.machine
local mode=assert(_G.nse2_fdn_record_mode)
local sequences={write='MDDDMDDMDMDMDM5678M2M12345M',read='MDDDMDDMDMDMMMH',wrong='MDDDMDDMDMDMDM5675M',
    edit='MDDDMDDMDMDMMMMM5678MMC6M',['read-edited']='MDDDMDDMDMDMMMH',
    erase='MDDDMDDMDMDMMMMDM5678MM',cancel='MDDDMDDMDMDMMMMDM5678MC',empty='MDDDMDDMDMDMMM'}
local sequence=assert(sequences[mode])
local keys={M={1,4},D={0,8},H={4,16},C={1,2},['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},['5']={3,4},['6']={4,4},['7']={2,8},['8']={3,8}}
local events={}
for i=1,#sequence do
    local key=sequence:sub(i,i)
    local cell=keys[key]
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'missing FDN record contact')
    local when=18.5+(i-1)*.35
    events[#events+1]={when,field,1}
    events[#events+1]={when+(key=='H' and 4 or .15),field,0}
end
local event=1
local captured=false
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local step=events[event]
        step[2]:set_value(step[3])
        machine:logerror(string.format('nse2_fdn_record_contact: event=%d down=%d t=%.9f physical_only=1\n',event,step[3],now))
        event=event+1
    end
    local capture_time=mode=='empty' and 24 or ((mode=='read' or mode=='read-edited') and 26.2 or 29.7)
    if not captured and now>=capture_time then
        captured=true
        machine.screens[':screen']:snapshot('nse2_fdn_final.png')
    end
end,'NSE-2 physical FDN record lifecycle')
