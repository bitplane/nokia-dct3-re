-- Local FDN policy probe: physical digits/Call, never a network response.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_security_fixture.lua')
local machine=manager.machine
local mode=assert(_G.nse2_fdn_dial_mode)
assert(mode=='listed' or mode=='unlisted' or mode=='off-control')
local sequence=mode=='listed' and '12346M' or '54321M'
local cells={M={1,4},['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},['5']={3,4},['6']={4,4}}
local events={}
for i=1,#sequence do
    local cell=cells[sequence:sub(i,i)]
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'missing dial contact')
    local when=18.5+(i-1)*.35
    events[#events+1]={when,field,1}
    events[#events+1]={when+.15,field,0}
end
local event=1
local digits,result=false,false
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local step=events[event]
        step[2]:set_value(step[3])
        machine:logerror(string.format('nse2_fdn_dial_contact: event=%d down=%d t=%.9f physical_only=1\n',event,step[3],now))
        event=event+1
    end
    if not digits and now>=19.95 then
        digits=true
        machine.screens[':screen']:snapshot('nse2_fdn_digits.png')
    end
    if not result and now>=22 then
        result=true
        machine.screens[':screen']:snapshot('nse2_fdn_final.png')
    end
end,'NSE-2 physical local FDN dialing contrast')
