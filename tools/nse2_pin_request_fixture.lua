-- PIN is collected before selecting On/Off; only physical contacts are driven.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
local mode=assert(_G.nse2_pin_request_mode)
assert(mode=='on' or mode=='off' or mode=='cold-off')
if mode=='off' then
    _G.nse2_pin_value='1234'
    _G.nse2_pin_phone_auth=true
    dofile(directory..'nse2_pin_fixture.lua')
else
    dofile(directory..'nse2_security_fixture.lua')
end
local machine=manager.machine
local sequence=mode=='cold-off' and '' or ('MDDDMDDMM1234M'..(mode=='on' and 'UM' or 'DM'))
local cells={M={1,4},D={0,8},U={1,16},['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4}}
local events={}
for i=1,#sequence do
    local cell=cells[sequence:sub(i,i)]
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'missing PIN request contact')
    local when=18.5+(i-1)*.35
    events[#events+1]={when,field,1}
    events[#events+1]={when+.15,field,0}
end
local event=1
local captured=false
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while event<=#events and now>=events[event][1] do
        local step=events[event]
        step[2]:set_value(step[3])
        machine:logerror(string.format('nse2_pin_request_contact: event=%d down=%d t=%.9f physical_only=1\n',event,step[3],now))
        event=event+1
    end
    if not captured and now>=29.7 then
        captured=true
        machine.screens[':screen']:snapshot('nse2_pin1_change_result.png')
    end
end,'NSE-2 physical PIN request lifecycle')
