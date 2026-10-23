-- Read-only native service cadence; no completion or receive traffic supplied.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
-- C54x opcode reads expose PC one word ahead in the validated live run.
-- Entry reads are fetch observations, not proof of instruction retirement.
local retained,last={},{}
local mcu=assert(machine.devices[':maincpu'])
local host_posts=0
local worker_objects=0
retained[#retained+1]=mcu.spaces.program:install_read_tap(0x28de3c,0x28de43,'word_worker_object',function()
    if mcu.state.R15.value~=0x28de34 then return end
    worker_objects=worker_objects+1
    if worker_objects>4 then return end
    local pointer=mcu.state.R4.value
    if not ((pointer>=0x100000 and pointer+24<=0x110000) or (pointer>=0x200000 and pointer+24<=0x400000)) then
        machine:logerror(string.format('nse2_word_worker_unqualified: pc=%08x pointer=%08x observer_writes=0\n',mcu.state.R15.value,pointer))
        return
    end
    local bytes={}
    for i=0,23 do bytes[#bytes+1]=string.format('%02x',mcu.spaces.program:read_u8(pointer+i)) end
    machine:logerror(string.format('nse2_word_worker_object: n=%d pc=%08x pointer=%08x bytes=%s t=%.9f observer_writes=0\n',worker_objects,mcu.state.R15.value,pointer,table.concat(bytes),machine.time:as_double()))
end)
local object_writes=0
retained[#retained+1]=mcu.spaces.program:install_write_tap(0x101438,0x101447,'word_discovery_object',function(address,value,mask)
    object_writes=object_writes+1
    if object_writes>32 then return end
    machine:logerror(string.format('nse2_word_object_write: address=%08x value=%08x mask=%08x pc=%08x lr=%08x t=%.9f observer_writes=0\n',address,value,mask,mcu.state.R15.value,mcu.state.R14.value,machine.time:as_double()))
    if mcu.state.R15.value==0x279a72 then
        local sp=mcu.state.R13.value
        local source=mcu.state.R7.value
        assert(sp>=0x100000 and sp+0x3c<=0x110000,'fragment wrapper stack outside MCU RAM')
        assert(source>=0x100000 and source+8<=0x110000,'fragment source outside MCU RAM')
        local bytes={}
        for i=0,7 do bytes[#bytes+1]=string.format('%02x',mcu.spaces.program:read_u8(source+i)) end
        machine:logerror(string.format('nse2_word_fragment_source: source=%08x bytes=%s caller=%08x t=%.9f observer_writes=0\n',source,table.concat(bytes),mcu.spaces.program:read_u32(sp+0x38),machine.time:as_double()))
    end
end)
retained[#retained+1]=mcu.spaces.program:install_write_tap(0x100a4,0x100a7,'word_host_post',function(_,value,mask)
    -- DSP 0852 / MCU 100a4 is the producer; 0853 is the DSP consumer.
    if mask&0xffff0000==0 then return end
    host_posts=host_posts+1
    if host_posts>16 then return end
    machine:logerror(string.format('nse2_word_host_post: n=%d value=%08x mask=%08x pc=%08x lr=%08x t=%.9f observer_writes=0\n',host_posts,value,mask,mcu.state.R15.value,mcu.state.R14.value,machine.time:as_double()))
    if host_posts==3 or host_posts==4 then
        local sp=mcu.state.R13.value
        assert(sp>=0x100000 and sp+64<=0x110000,'host poster stack outside MCU RAM')
        local stack={}
        for i=0,60,4 do stack[#stack+1]=string.format('%08x',mcu.spaces.program:read_u32(sp+i)) end
        machine:logerror(string.format('nse2_word_host_stack: n=%d sp=%08x words=%s observer_writes=0\n',host_posts,sp,table.concat(stack,',')))
    end
end)
local queue_writes=0
local builder_entries=0
retained[#retained+1]=cpu.spaces.program:install_read_tap(0x8db4,0x8db4,'word_builder_caller',function()
    local pc=cpu.state.PC.value
    if pc~=0x8db4 and pc~=0x8db5 then return end
    builder_entries=builder_entries+1
    if builder_entries>16 then return end
    local sp=cpu.state.SP.value
    local stack={}
    for i=0,3 do stack[#stack+1]=string.format('%04x',cpu.spaces.data:read_u16((sp+i)&0xffff)) end
    machine:logerror(string.format('nse2_word_builder_entry: n=%d a=%x b=%x ar0=%04x ar2=%04x sp=%04x stack=%s t=%.9f observer_writes=0\n',builder_entries,cpu.state.A.value,cpu.state.B.value,cpu.state.AR0.value,cpu.state.AR2.value,sp,table.concat(stack,','),machine.time:as_double()))
end)
retained[#retained+1]=cpu.spaces.data:install_write_tap(0x700,0x70a,'word_queue_build',function(address,value,mask)
    queue_writes=queue_writes+1
    if queue_writes>64 then return end
    machine:logerror(string.format('nse2_word_queue_write: address=%04x value=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',address,value,mask,cpu.state.PC.value,machine.time:as_double()))
end)
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('nse2_word_queue_summary: writes=%d cap=64 observer_writes=0\n',queue_writes))
end)
for _,address in ipairs({0x3924,0x3f4e,0x8dd0,0x9232}) do
    local reads,exact,plusone,other=0,0,0,0
    retained[#retained+1]=cpu.spaces.program:install_read_tap(address,address,'word_service_fetch',function()
        if _G.rom4_observation_read then return end
        local pc=cpu.state.PC.value
        reads=reads+1
        if pc==address then exact=exact+1 elseif pc==address+1 then plusone=plusone+1 else other=other+1 end
    end)
    retained[#retained+1]=emu.add_machine_stop_notifier(function()
        machine:logerror(string.format('nse2_word_service_fetch_summary: address=%04x reads=%d exact=%d plusone=%d other=%d observer_writes=0\n',address,reads,exact,plusone,other))
    end)
end
for _,address in ipairs({0x06b3,0x06c4}) do
    local count,minimum,maximum=0,nil,nil
    local increments,increment_last,increment_min,increment_max=0,nil,nil,nil
    local writers={}
    retained[#retained+1]=cpu.spaces.data:install_write_tap(address,address,'word_service_counter',function(_,value,mask)
        local now=machine.time:as_double()
        count=count+1
        local pc=cpu.state.PC.value
        writers[pc]=(writers[pc] or 0)+1
        if pc==(address==0x06b3 and 0x8e74 or 0x8e71) then
            increments=increments+1
            if increment_last then
                local delta=now-increment_last
                increment_min=math.min(increment_min or delta,delta)
                increment_max=math.max(increment_max or delta,delta)
            end
            increment_last=now
        end
        if last[address] then
            local delta=now-last[address]
            minimum=math.min(minimum or delta,delta)
            maximum=math.max(maximum or delta,delta)
        end
        last[address]=now
        if count<=4 then
            machine:logerror(string.format('nse2_word_service_counter: address=%04x value=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',address,value,mask,cpu.state.PC.value,now))
        end
    end)
    retained[#retained+1]=emu.add_machine_stop_notifier(function()
        machine:logerror(string.format('nse2_word_service_counter_summary: address=%04x writes=%d min=%.9f max=%.9f observer_writes=0\n',address,count,minimum or 0,maximum or 0))
        machine:logerror(string.format('nse2_word_service_increment_summary: address=%04x increments=%d min=%.9f max=%.9f observer_writes=0\n',address,increments,increment_min or 0,increment_max or 0))
        local sites={}
        for pc in pairs(writers) do sites[#sites+1]=pc end
        table.sort(sites)
        for _,pc in ipairs(sites) do
            machine:logerror(string.format('nse2_word_service_writer: address=%04x pc=%04x writes=%d observer_writes=0\n',address,pc,writers[pc]))
        end
    end)
end
_G.nse2_word_service_observer=retained
