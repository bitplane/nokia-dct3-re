-- Own matrix contacts and read-only validator breakpoints; no firmware writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_sim_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local start=_G.nsm1_phone_code_start or 9
machine.debugger:command('bpset 2bcde8,1,{logerror "nsm1_security_submit: text=%08x%08x caller=%08x observer_writes=0\\n",d@r0,d@(r0+4),r14;g}')
machine.debugger:command('bpset 2bcd98,1,{logerror "nsm1_security_gate: match=%08x retry_pointer=%08x retry=%02x observer_writes=0\\n",r0,r1,b@r1;g}')
machine.debugger:command('bpset 2bcde6,1,{logerror "nsm1_security_accepted: result=%08x observer_writes=0\\n",r0;g}')
local cells={{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{1,2,0x19}}
for _,cell in ipairs(_G.nsm1_followup_contacts or {{1,2,0x19}}) do cells[#cells+1]=cell end
local events={}
for index,cell in ipairs(cells) do
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'own physical key unavailable')
    local row=0
    while (1<<row)~=cell[2] do row=row+1 end
    assert(memory:read_u8(0x2e0b18+row*5+cell[1])==cell[3],'own key table changed')
    local time=index>=7 and start+6+(index-7)*0.8 or start+(index-1)*0.5
    events[#events+1]={time,field,1,cell[3]}
    events[#events+1]={time+0.2,field,0,cell[3]}
end
local index,snap=1,1
local times={10,12,14,16,18,20}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[index] and now>=events[index][1] do
        local event=events[index];event[2]:set_value(event[3])
        machine:logerror(string.format('nsm1_security_contact: key=%02x down=%d t=%.6f firmware_writes=0\n',event[4],event[3],now))
        index=index+1
    end
    if times[snap] and now>=times[snap] then
        machine.screens[':screen']:snapshot(string.format('nsm1_security_%d.png',times[snap]))
        snap=snap+1
    end
end)
