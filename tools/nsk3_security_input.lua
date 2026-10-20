-- Own table2c7a30: physical digits12345 and candidate command19, no guest writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_sim_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local start=_G.nsk3_phone_code_start or 9
machine.debugger:command('bpset 2a64c0,temp8<100,{temp8=temp8+1;logerror "nsk3_security_gate: match=%08x retry_pointer=%08x retry=%02x cycles=%016x observer_writes=0\\n",r0,r1,b@r1,totalcycles;g}')
machine.debugger:command('bpset 2a6510,temp8<100 && r0>=100000 && r0<10fff8,{temp8=temp8+1;logerror "nsk3_security_submit: pointer=%08x text=%08x%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,d@r0,d@(r0+4),r14,totalcycles;g}')
machine.debugger:command('bpset 2a650e,temp8<100,{temp8=temp8+1;logerror "nsk3_security_accepted: result=%08x cycles=%016x observer_writes=0\\n",r0,totalcycles;g}')
local cells={{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{1,2,0x19}}
if _G.nsk3_followup_menu then cells[#cells+1]={1,2,0x19} end
for _,cell in ipairs(_G.nsk3_followup_contacts or {}) do cells[#cells+1]=cell end
local events={}
for i,cell in ipairs(cells) do
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'own physical contact unavailable')
    local row=0
    while (1<<row)~=cell[2] and row<5 do row=row+1 end
    assert(memory:read_u8(0x2c7a30+row*5+cell[1])==cell[3],'own logical-key table changed')
    local when=i>=7 and start+4.5+(i-7)*0.8 or start+(i-1)*0.5
    events[#events+1]={when,field,1,cell}
    events[#events+1]={when+0.2,field,0,cell}
end
local next_event=1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[next_event] and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk3_security_contact: key=%02x down=%d column=%d mask=%02x t=%.6f firmware_writes=0\n',event[4][3],event[3],event[4][1],event[4][2],now))
        next_event=next_event+1
    end
end)
