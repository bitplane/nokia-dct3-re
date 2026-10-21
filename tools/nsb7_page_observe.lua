-- Observe upper-layer delivery after PCH decoding; never inject messages.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_lapdm_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local count=0
_G.nsb7_page_9c8_source=memory:install_read_tap(0x22e28c,0x22e28f,'nsb7_9c8_source',function()
    if cpu.state['R15'].value~=0x22e28c or cpu.state['R0'].value~=0x9c8 then return end
    machine:logerror(string.format('nsb7_9c8_source: caller=%08x task=%u t=%.9f observer_writes=0\n',
        cpu.state['R14'].value,memory:read_u8(0x100022),machine.time:as_double()))
end)
local tap=memory:install_read_tap(0x2898e8,0x2898eb,'nsb7_page_posts',function()
    local time=machine.time:as_double()
    if cpu.state['R15'].value~=0x2898e8 or time<3 or time>19 or count>=256 then return end
    local target=cpu.state['R0'].value
    if target~=13 and target~=16 and target~=17 and target~=18 then return end
    local object=cpu.state['R1'].value
    if object<0x100000 or object>=0x17ff00 then return end
    count=count+1
    local bytes={}
    for index=0,31 do bytes[#bytes+1]=string.format('%02x',memory:read_u8(object+index)) end
    machine:logerror(string.format('nsb7_page_post: index=%u source=%u target=%u lr=%08x data=%s t=%.9f observer_writes=0\n',
        count,memory:read_u8(0x100022),cpu.state['R0'].value,cpu.state['R14'].value,
        table.concat(bytes),time))
    if memory:read_u16(object)==0x7d4 and cpu.state['R14'].value==0x200e39 then
        machine:logerror(string.format('nsb7_rr_7d4_caller: caller=%08x t=%.9f observer_writes=0\n',
            memory:read_u32(cpu.state['R13'].value+12),time))
    end
    if memory:read_u16(object)==0x5eb then
        local pointer=memory:read_u32(object+8)
        if pointer>=0x100000 and pointer<0x17ff00 then
            local body={}
            for index=0,27 do body[#body+1]=string.format('%02x',memory:read_u8(pointer+index)) end
            machine:logerror(string.format('nsb7_page_indication: pointer=%08x data=%s t=%.9f observer_writes=0\n',
                pointer,table.concat(body),time))
        end
    end
end)
_G.nsb7_page_service_completion=memory:install_read_tap(0x2a3644,0x2a3647,'nsb7_service_completion',function()
    if cpu.state['R15'].value~=0x2a3646 then return end
    local pending=memory:read_u32(0x130854)
    if pending<0x100000 or pending>=0x17ff00 then return end
    machine:logerror(string.format('nsb7_service_completion: pending=%08x tag=%04x caller=%08x t=%.9f observer_writes=0\n',
        pending,memory:read_u16(pending),cpu.state['R14'].value,machine.time:as_double()))
end)
local snapshots=coroutine.create(function()
    for _,target in ipairs({7,10.6,14.7,16,24}) do
        assert(emu.wait(target-machine.time:as_double()))
        local root=memory:read_u32(0x12fbd8)
        local pending=memory:read_u32(0x130854)
        local state,tag=0xffffffff,0xffff
        if root>=0x100000 and root<0x17ff00 then state=memory:read_u32(root) end
        if pending>=0x100000 and pending<0x17ff00 then tag=memory:read_u16(pending) end
        machine:logerror(string.format('nsb7_service_snapshot: root=%08x state=%08x pending=%08x tag=%04x mode=%02x t=%.9f observer_writes=0\n',
            root,state,pending,tag,memory:read_u8(0x130310),machine.time:as_double()))
    end
end)
_G.nsb7_service_snapshots=snapshots
assert(coroutine.resume(snapshots))
_G.nsb7_page_9c8_completion=memory:install_read_tap(0x232058,0x23205b,'nsb7_9c8_completion',function()
    if cpu.state['R15'].value~=0x23205a then return end
    machine:logerror(string.format('nsb7_9c8_completion: event=%04x record=%08x t=%.9f observer_writes=0\n',
        cpu.state['R0'].value,cpu.state['R4'].value,machine.time:as_double()))
end)
_G.nsb7_page_flag06_trigger=memory:install_read_tap(0x260a34,0x260a37,'nsb7_rr_flag06_trigger',function()
    if cpu.state['R15'].value~=0x260a34 then return end
    machine:logerror(string.format('nsb7_rr_flag06_trigger: event=%04x report=%04x t=%.9f observer_writes=0\n',
        memory:read_u32(cpu.state['R13'].value+4),cpu.state['R0'].value,machine.time:as_double()))
end)
_G.nsb7_page_post_tap=tap
local receives=0
_G.nsb7_page_receive_tap=memory:install_read_tap(0x289b3c,0x289b3f,'nsb7_page_receive',function()
    local time=machine.time:as_double()
    if cpu.state['R15'].value~=0x289b3c or memory:read_u8(0x100022)~=17 or
            time<15 or time>17 or receives>=64 then return end
    receives=receives+1
    machine:logerror(string.format('nsb7_page_receive: lr=%08x t=%.9f observer_writes=0\n',
        cpu.state['R14'].value,time))
end)
local identity_taps={}
for _,pc in ipairs({0x274124,0x2741dc,0x2741f6,0x274e82,0x2b3130}) do
    local hits=0
    identity_taps[#identity_taps+1]=memory:install_read_tap(pc-pc%4,pc-pc%4+3,'nsb7_page_identity_'..pc,function()
        if cpu.state['R15'].value~=pc or hits>=24 then return end
        hits=hits+1
        local pointer=cpu.state[pc==0x274124 and 'R1' or 'R4'].value
        local bytes={}
        if pointer>=0x100000 and pointer<0x17ff00 then
            for i=0,8 do bytes[#bytes+1]=string.format('%02x',memory:read_u8(pointer+i)) end
        end
        local stack={}
        for i=0,12 do stack[#stack+1]=string.format('%02x',memory:read_u8(cpu.state['R13'].value+i)) end
        machine:logerror(string.format('nsb7_page_identity: pc=%08x r0=%08x r2=%08x data=%s stack=%s t=%.9f observer_writes=0\n',
            pc,cpu.state['R0'].value,cpu.state['R2'].value,table.concat(bytes),table.concat(stack),machine.time:as_double()))
    end)
end
_G.nsb7_page_identity_taps=identity_taps
_G.nsb7_page_event_return=memory:install_read_tap(0x2b31a8,0x2b31ab,'nsb7_page_event_return',function()
    if cpu.state['R15'].value~=0x2b31aa or cpu.state['R0'].value~=0x83e then return end
    machine:logerror(string.format('nsb7_page_event_return: event=083e caller=%08x record=%08x t=%.9f observer_writes=0\n',
        memory:read_u32(cpu.state['R13'].value+20),cpu.state['R4'].value,machine.time:as_double()))
end)
local dispatch_taps={}
for _,pc in ipairs({0x260c4c,0x260c16}) do
    dispatch_taps[#dispatch_taps+1]=memory:install_read_tap(pc-pc%4,pc-pc%4+3,'nsb7_page_dispatch_'..pc,function()
        if cpu.state['R15'].value~=pc or memory:read_u32(cpu.state['R13'].value+4)~=0x83e then return end
        local record=cpu.state['R4'].value
        if record<0x100000 or record>=0x17ff00 then return end
        machine:logerror(string.format('nsb7_page_dispatch: pc=%08x event=083e record=%08x flag0b=%02x saved=%08x flag03=%02x flag06=%02x api1=%02x t=%.9f observer_writes=0\n',
            pc,record,memory:read_u8(record+0xb),memory:read_u32(record+0x28),
            memory:read_u8(record+3),memory:read_u8(record+6),memory:read_u8(0x12d213),machine.time:as_double()))
    end)
end
_G.nsb7_page_dispatch_taps=dispatch_taps
local rssi_count=0
_G.nsb7_page_rssi_forward=memory:install_read_tap(0x2e49b4,0x2e49b7,'nsb7_serving_rssi_forward',function()
    if cpu.state['R15'].value~=0x2e49b4 or rssi_count>=24 then return end
    local object=cpu.state['R0'].value
    if object<0x100000 or object>=0x17ff00 then return end
    rssi_count=rssi_count+1
    local bytes={}
    for i=0,11 do bytes[#bytes+1]=string.format('%02x',memory:read_u8(object+i)) end
    machine:logerror(string.format('nsb7_serving_rssi_forward: data=%s t=%.9f observer_writes=0\n',
        table.concat(bytes),machine.time:as_double()))
end)
local flag_writes=0
_G.nsb7_page_flag06_watch=memory:install_write_tap(0x12c980,0x12c983,'nsb7_rr_flag06',function(_,value,mask)
    if mask&0xff00==0 or flag_writes>=32 then return end
    flag_writes=flag_writes+1
    machine:logerror(string.format('nsb7_rr_flag06: value=%02x pc=%08x task=%u t=%.9f observer_writes=0\n',
        (value>>8)&255,cpu.state['R15'].value,memory:read_u8(0x100022),machine.time:as_double()))
end)
