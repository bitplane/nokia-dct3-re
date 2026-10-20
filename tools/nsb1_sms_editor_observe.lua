-- Bounded read-only dispatch probe, not SMS acceptance. Requires -debug -debugger none.
-- Retire the breakpoints once the main-editor input contract is accepted.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_security_fixture.lua')
local machine = manager.machine
local debugger = assert(machine.debugger, 'editor dispatch probe needs the debugger')
debugger:command('focus :maincpu')
debugger:command('temp5=0')
for _, address in ipairs({'25c8a0','25c8b6','25c8c4','25c8ce','25c92c'}) do
    debugger:command('bpset ' .. address .. ',temp5<40,{temp5=temp5+1;logerror "nsb1_sms_init: pc=%08x r0=%08x r1=%08x r4=%08x lr=%08x validity=%04x cycles=%016x observer_writes=0\\n",pc,r0,r1,r4,r14,w@1097e2,totalcycles;g}')
end
debugger:command('temp6=0')
debugger:command('bpset 27389c,temp6<10,{temp6=temp6+1;logerror "nsb1_sms_resource: source=%08x destination=%08x bytes=%08x base=%08x observer_writes=0\\n",r1,r0,r2,d@10642c;g}')
debugger:command('bpset 25c4f8,temp6<10,{logerror "nsb1_sms_resource_header: version=%08x observer_writes=0\\n",r0;g}')
debugger:command('temp7=0')
debugger:command('temp8=0')
debugger:command('bpset 2ab0e6,r1<=10fced&&r1+r2>10fced&&temp8<8,{temp8=temp8+1;logerror "nsb1_sms_preference_read: eeprom=%08x destination=%08x bytes=%08x caller=%08x observer_writes=0\\n",r0,r1,r2,r14;g}')
for _, address in ipairs({'272c70','272c9e','272ca2'}) do
    debugger:command('bpset ' .. address .. ',temp7<20,{temp7=temp7+1;logerror "nsb1_sms_language: pc=%08x r0=%08x language=%02x state=%02x selected_base=%08x observer_writes=0\\n",pc,r0,b@10fced,b@106424,d@10642c;g}')
end
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
        debugger:command('temp3=0')
        debugger:command('temp4=0')
        debugger:command('bpset 25e20c,r14==272b87&&temp4<10,{temp4=temp4+1;logerror "nsb1_sms_text_api: object=%08x op=%08x argument=%04x validity=%04x observer_writes=0\\n",r0,d@r1,w@(r1+4),w@1097e2;g}')
        debugger:command('bpset 272b86,temp4<10,{logerror "nsb1_sms_text_return: result=%08x observer_writes=0\\n",r0;g}')
        for _, address in ipairs({'28e4ca','28e4d0','28e4d6'}) do
            debugger:command('bpset ' .. address .. ',temp3<20,{temp3=temp3+1;logerror "nsb1_sms_translate: pc=%08x r0=%08x mode=%02x key=%08x editor=%08x buffer=%08x descriptor=%08x observer_writes=0\\n",pc,r0,b@106424,d@10b118,d@10c2d8,d@(10c2d8+10),d@(10c2d8+18);g}')
        end
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
local language_writes = 0
_G.nsb1_sms_language_write = cpu.spaces['program']:install_write_tap(0x10fcec,0x10fcef,'nsb1_sms_language_write',function(offset,data,mask)
    if language_writes < 16 then
        language_writes = language_writes+1
        machine:logerror(string.format('nsb1_sms_language_write: data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
            data,mask,cpu.state['R15'].value,machine.time:as_double()))
    end
end)
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
