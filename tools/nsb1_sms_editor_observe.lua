-- Bounded read-only dispatch probe, not SMS acceptance. Requires -debug -debugger none.
-- Retire the breakpoints once the main-editor input contract is accepted.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_security_fixture.lua')
local machine = manager.machine
local keys = {}
for _, definition in ipairs({{'menu',1,4},{'one',2,2},{'two',3,2},{'three',4,2}}) do
    for _, field in pairs(machine.ioport.ports[':COL.' .. definition[2]].fields) do
        if field.mask == definition[3] then keys[definition[1]] = field end
    end
    assert(keys[definition[1]], 'own physical editor contact missing')
end
local events = {}
for index, event in ipairs({{26,'menu'},{28,'two'},{30,'one'},{32,'three'},{38,'two'}}) do
    events[#events+1] = {event[1],keys[event[2]],1,index,event[2]}
    events[#events+1] = {event[1]+0.2,keys[event[2]],0,index,event[2]}
end
local next_event, next_frame, armed = 1, 1, false
local times = {35,39}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if not armed and now >= 37 then
        armed = true
        local debugger = assert(machine.debugger, 'editor dispatch probe needs the debugger')
        debugger:command('focus :maincpu')
        debugger:command('temp0=0')
        debugger:command('temp1=0')
        debugger:command('temp2=0')
        -- Task 5 strips the packed argument count before this handler pipeline.
        -- Observe each return, not a guessed task-6 text-delivery contract.
        for _, address in ipairs({'2abe1c','2abe2a','2abe30','2abe36','2abe3c','2abe42','2abe48','2abe4e','2abe54'}) do
            debugger:command('bpset ' .. address .. ',temp2<80,{temp2=temp2+1;logerror "nsb1_sms_stage: pc=%08x r0=%08x stored_status=%04x cycles=%016x observer_writes=0\\n",pc,r0,w@(r4+6),totalcycles;g}')
        end
        -- Debugger numeric literals are hexadecimal: 40 bounds each stream to 64.
        -- Eight bytes fit both observed allocations; bytes 2/3 may be padding.
        debugger:command('bpset 268a94,(r0==5||r0==6)&&r1>=100000&&r1<110000&&temp0<40,{temp0=temp0+1;logerror "nsb1_sms_post: task=%08x object=%08x words=%08x/%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,d@r1,d@(r1+4),r14,totalcycles;g}')
        debugger:command('bpset 2af79e,r0!=3e&&temp1<40,{temp1=temp1+1;logerror "nsb1_sms_decode: key=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
    end
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsb1_sms_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[4],event[5],event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #times and now >= times[next_frame] then
        machine.screens[':screen']:snapshot(string.format('nsb1_sms_editor_%d.png',next_frame))
        next_frame = next_frame+1
    end
end, 'NSB-1 SMS editor dispatch investigation')
local cpu = machine.devices[':maincpu']
local status_reads = 0
_G.nsb1_sms_status_read = cpu.spaces['program']:install_read_tap(0x101474,0x101477,'nsb1_sms_status_read',function(offset,data,mask)
    local status, pc = (data >> 16) & 0xffff, cpu.state['R15'].value
    if machine.time:as_double() >= 37 and status_reads < 16 and
            (status == 0x40c8 or status == 0x40c9) and pc ~= 0x268a94 then
        status_reads = status_reads+1
        machine:logerror(string.format('nsb1_sms_status_read: status=%04x mask=%08x pc=%08x caller=%08x t=%.9f observer_writes=0\n',
            status,mask,pc,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
