-- Physical FDN menu contacts; phase selection changes inputs, never firmware state.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_security_fixture.lua')
local machine=manager.machine
local phase=assert(_G.nse2_fdn_phase,'FDN phase required')
assert(phase>=0 and phase<=3)
local sequences={'MDDDMDDMDMUM5678M','MDDDMDDMD','MDDDMDDMDMDM5678M','MDDDMDDMD'}
local sequence=sequences[phase+1]
local keys={M={1,4},D={0,8},U={1,16},['5']={3,4},['6']={4,4},['7']={2,8},['8']={3,8}}
local events={}
for i=1,#sequence do
    local cell=keys[sequence:sub(i,i)]
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'missing physical FDN contact')
    events[#events+1]={18.5+(i-1)*.35,field,1}
    events[#events+1]={18.65+(i-1)*.35,field,0}
end
local event=1
local captured=false
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local step=events[event]
        step[2]:set_value(step[3])
        machine:logerror(string.format('nse2_fdn_contact: event=%d down=%d t=%.9f physical_only=1\n',event,step[3],now))
        event=event+1
    end
    if not captured and now>=29.7 then
        captured=true
        machine.screens[':screen']:snapshot('nse2_fdn_final.png')
    end
end,'NSE-2 physical FDN lifecycle')
