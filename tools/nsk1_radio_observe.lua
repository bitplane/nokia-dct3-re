-- Passive shared-ring census plus a physical ordinary Call/End attempt.
_G.nsk1_security_code_only = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local dsp = machine.devices[':dsp_c54x:cpu']
local memory = cpu.spaces.program
machine.debugger:command('focus :maincpu')
machine.debugger:command('temp7=0')
machine.debugger:command('bpset 27341a,d@r13>=100000&&d@r13<110000&&b@(d@r13+3)==8b&&temp7<8,{temp7=temp7+1;logerror "nsk1_radio_receive_return: object=%08x header=%08x caller=%08x observer_writes=0\\n",d@r13,d@(d@r13),d@(r13+14);g}')
machine.debugger:command('temp6=0')
machine.debugger:command('bpset 26a440,temp6<8,{temp6=temp6+1;logerror "nsk1_radio_route: object=%08x header=%08x caller=%08x observer_writes=0\\n",r0,d@r0,r14;g}')
machine.debugger:command('bpset 255268,r0==b&&r1>=100000&&r1<110000&&w@r1==13b8&&temp6<16,{temp6=temp6+1;logerror "nsk1_radio_post: task=%02x object=%08x header=%08x caller=%08x observer_writes=0\\n",r0,r1,d@r1,r14;g}')
machine.debugger:command('temp5=0')
for _,address in ipairs({'218996','2189d6','218a02'}) do
    machine.debugger:command('bpset '..address..',temp5<12,{temp5=temp5+1;logerror "nsk1_radio_consumer: pc=%08x object=%08x owner=%08x state=%02x phase=%02x cache=%08x observer_writes=0\\n",pc,r5,r6,b@(r6+14),b@(r6+19),d@(r4+28);g}')
end
machine.debugger:command('temp4=0')
machine.debugger:command('bpset 215d4c,temp4<8,{temp4=temp4+1;logerror "nsk1_radio_candidate: object=%08x caller=%08x observer_writes=0\\n",r0,r14;g}')
machine.debugger:command('temp3=0')
for _,address in ipairs({'2177a0','218d20'}) do
    machine.debugger:command('bpset '..address..',temp3<12,{temp3=temp3+1;logerror "nsk1_radio_activation: pc=%08x r0=%08x caller=%08x observer_writes=0\\n",pc,r0,r14;g}')
end
machine.debugger:command('temp2=0')
machine.debugger:command('bpset 26def0,r0==43f&&temp2<8,{temp2=temp2+1;logerror "nsk1_radio_construct: input=%04x field2=%02x field3=%02x caller=%08x observer_writes=0\\n",r0,b@10be7c,b@10be7b,r14;g}')
machine.debugger:command('bpset 24df64,temp2<16,{temp2=temp2+1;logerror "nsk1_radio_producer: pc=%08x caller=%08x observer_writes=0\\n",pc,r14;g}')
machine.debugger:command('temp1=0')
machine.debugger:command('bpset 221236,r0==a30&&temp1<16,{temp1=temp1+1;logerror "nsk1_radio_upstream: input=%04x caller=%08x observer_writes=0\\n",r0,r14;g}')
machine.debugger:command('bpset 2081d8,r0==834&&temp1<24,{temp1=temp1+1;logerror "nsk1_radio_request: input=%04x field2=%02x field3=%02x caller=%08x observer_writes=0\\n",r0,b@10a463,b@10a460,r14;g}')
machine.debugger:command('bpset 20c5e0,temp1<32,{temp1=temp1+1;logerror "nsk1_radio_selector: input=%04x caller=%08x observer_writes=0\\n",r9,r14;g}')
machine.debugger:command('temp8=0')
machine.debugger:command('bpset 221236,temp8<40,{temp8=temp8+1;logerror "nsk1_radio_controller_output: input=%04x caller=%08x observer_writes=0\\n",r0,r14;g}')
machine.debugger:command('bpset 26dc28,temp8<40,{temp8=temp8+1;logerror "nsk1_radio_task16_output: input=%04x r1=%08x r2=%08x r3=%08x caller=%08x observer_writes=0\\n",r0,r1,r2,r3,r14;g}')
machine.debugger:command('temp9=0')
machine.debugger:command('bpset 22137e,r0>=100000&&r0<110000&&temp9<40,{temp9=temp9+1;logerror "nsk1_radio_controller_receive: object=%08x header=%08x caller=%08x observer_writes=0\\n",r0,d@r0,d@(r13+10);g}')
machine.debugger:command('bpset 26e642,r0>=100000&&r0<110000&&temp9<40,{temp9=temp9+1;logerror "nsk1_radio_task16_receive: object=%08x header=%08x caller=%08x waitmode=%08x observer_writes=0\\n",r0,d@r0,d@(r13+10),r5;g}')
machine.debugger:command('bpset 26e788,temp9<40,{temp9=temp9+1;logerror "nsk1_radio_task16_return: input=%04x caller=%08x observer_writes=0\\n",r6,d@(r13+10);g}')
machine.debugger:command('bpset 24dff8,r11==7d6,{logerror "nsk1_radio_task16_dispatch: input=%04x state=%04x owner=%08x target=%08x observer_writes=0\\n",r11,r0,r4,d@(r1+r0*4);g}')
machine.debugger:command('bpset 26def0,r0==3e9,{logerror "nsk1_radio_inner_construct: input=%04x caller=%08x observer_writes=0\\n",r0,r14;g}')
machine.debugger:command('bpset 255268,r0==a&&r1>=100000&&r1<110000&&w@r1==3e9,{logerror "nsk1_radio_inner_post: task=%02x object=%08x header=%08x control=%02x caller=%08x observer_writes=0\\n",r0,r1,d@r1,b@(r1+4),r14;g}')
machine.debugger:command('bpset 255268,r0==f&&r1>=100000&&r1<110000&&w@r1==9ec,{logerror "nsk1_radio_request_post: task=%02x object=%08x header=%08x caller=%08x observer_writes=0\\n",r0,r1,d@r1,r14;g}')
machine.debugger:command('bpset 2091b2,1,{logerror "nsk1_radio_mode_receive: object=%08x mode=%02x owner=%08x aux=%08x observer_writes=0\\n",r4,b@(r4+2),r5,r7;g}')
machine.debugger:command('bpset 20948a,temp2<40,{temp2=temp2+1;logerror "nsk1_radio_task15_receive_return: input=%04x mode=%02x flag=%02x caller=%08x observer_writes=0\\n",r6,b@(r5+1),b@(r5+2),d@(r13+14);g}')
machine.debugger:command('bpset 20c950,r9==9ec,{logerror "nsk1_radio_task15_dispatch: input=%04x state=%04x owner=%08x target=%08x observer_writes=0\\n",r9,r0,r4,d@(r1+r0*4);g}')
for _,address in ipairs({'20bbf4','20aa2e','20aa98','20ab48','20bd02'}) do
    machine.debugger:command('bpset '..address..',temp3<30,{temp3=temp3+1;logerror "nsk1_radio_task15_stage: pc=%08x input=%04x owner=%08x state=%04x mode=%02x caller=%08x observer_writes=0\\n",pc,r9,r4,w@(r4+42),b@(r4+1),r14;g}')
end
local counts = {tx=0,rx=0}
local observing = false
local function shared_word(address)
    observing = true
    local value = memory:read_u16(address)
    observing = false
    return value
end
local function packets(direction,old,new,start,finish,pc)
    -- This bounded census starts after the observed provisioning restart.
    -- Earlier producer reinitialization is not interpreted as queued packets.
    if machine.time:as_double() < 2 then return end
    if old < start or old >= finish or new < start or new >= finish or old == new then return end
    local cursor,walked = old,0
    local size = finish-start
    while cursor ~= new and walked < size do
        local header = shared_word(0x10000+cursor*2)
        local length = header >> 8
        local words = (length+3)//2
        local available = (new-cursor)%size
        if words > available then
            machine:logerror(string.format('nsk1_radio_ring_error: direction=%s old=%04x new=%04x header=%04x observer_writes=0\n',direction,old,new,header))
            return
        end
        counts[direction] = counts[direction]+1
        if counts[direction] <= 128 then
            local payload = {}
            for byte=0,length-1 do
                local word = shared_word(0x10000+2*(start+(cursor-start+1+byte//2)%size))
                payload[#payload+1] = string.format('%02x',byte%2 == 0 and word >> 8 or word & 255)
            end
            machine:logerror(string.format('nsk1_radio_packet: direction=%s index=%d type=%02x length=%d data=%s pc=%08x t=%.9f observer_writes=0\n',direction,counts[direction],header & 255,length,table.concat(payload),pc,machine.time:as_double()))
        end
        cursor,walked = start+(cursor-start+words)%size,walked+words
    end
end
local tx,rx = 0,0
local result_reads = 0
local status_reads = 0
local state_writes = 0
local lifecycle_writes = 0
_G.nsk1_radio_lifecycle = memory:install_write_tap(0x10a484,0x10a487,'nsk1_radio_lifecycle',function(_,data,mask)
    if mask & 0xffff ~= 0 and lifecycle_writes < 48 then
        lifecycle_writes = lifecycle_writes+1
        machine:logerror(string.format('nsk1_radio_lifecycle_write: state=%04x pc=%08x t=%.9f observer_writes=0\n',data & 0xffff,cpu.state['R15'].value,machine.time:as_double()))
    end
end)
_G.nsk1_radio_state = memory:install_write_tap(0x10716c,0x10716f,'nsk1_radio_state',function(_,data,mask)
    if mask & 0xff000000 ~= 0 and state_writes < 32 then
        state_writes = state_writes+1
        machine:logerror(string.format('nsk1_radio_state_write: value=%02x pc=%08x t=%.9f observer_writes=0\n',(data >> 24) & 255,cpu.state['R15'].value,machine.time:as_double()))
    end
end)
-- Observed allocator result in this fixture, not a whole allocator census.
_G.nsk1_radio_status = memory:install_read_tap(0x1030f4,0x1030f7,'nsk1_radio_status',function(_,data,mask)
    if not observing and machine.time:as_double() >= 39 and status_reads < 16
            and mask & 0xffff0000 ~= 0 and (data >> 16) & 0xffff == 0x13b8 then
        status_reads = status_reads+1
        machine:logerror(string.format('nsk1_radio_status_read: pc=%08x caller=%08x t=%.9f observer_writes=0\n',cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
_G.nsk1_radio_result = memory:install_read_tap(0x10100,0x101c7,'nsk1_radio_result',function(offset,data,mask)
    if observing or machine.time:as_double() < 2 or result_reads >= 16 then return end
    if ((mask & 0xffff0000 ~= 0 and (data >> 16) & 0xffff == 0xa28b) or
            (mask & 0xffff ~= 0 and data & 0xffff == 0xa28b)) then
        result_reads = result_reads+1
        machine:logerror(string.format('nsk1_radio_result_read: address=%08x pc=%08x caller=%08x t=%.9f observer_writes=0\n',offset,cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
_G.nsk1_radio_tx = memory:install_write_tap(0x100a4,0x100a7,'nsk1_radio_tx',function(_,data,mask)
    if mask & 0xffff0000 == 0 then return end
    local new = (data >> 16) & 0xffff
    packets('tx',tx,new,0,0x52,cpu.state['R15'].value)
    tx = new
end)
_G.nsk1_radio_rx = dsp.spaces.data:install_write_tap(0x08e4,0x08e4,'nsk1_radio_rx',function(_,data)
    packets('rx',rx,data,0x80,0xe4,dsp.state['PC'].value)
    rx = data
end)
local keys = {}
for name,cell in pairs({menu={1,4},five={3,4},zero={3,0x10},one={2,2}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name] = field end
    end
    assert(keys[name],'own call contact missing')
end
local sequence = {{21,'menu'}}
if not _G.nsk1_radio_no_call then
    for index,name in ipairs({'five','five','five','zero','one','zero','zero'}) do
        sequence[#sequence+1] = {26+(index-1)*0.6,name}
    end
    sequence[#sequence+1] = {32,'menu'}
    sequence[#sequence+1] = {52,'menu'}
end
local events = {}
for index,event in ipairs(sequence) do
    events[#events+1] = {event[1],event[2],1,index}
    events[#events+1] = {event[1]+0.2,event[2],0,index}
end
local index,frame = 1,1
local times = {30.5,33,36,50,59}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsk1_radio_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[4],event[2],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_radio_'..frame..'.png')
        frame = frame+1
        if frame > #times then
            machine:logerror(string.format('nsk1_radio_summary: tx=%d rx=%d cap=128 start=2 end=59 observer_writes=0\n',counts.tx,counts.rx))
        end
    end
end,'NSK-1 native radio boundary observation')
