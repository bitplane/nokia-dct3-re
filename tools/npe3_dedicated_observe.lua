-- Bounded native SEND_BLOCK integrity observation; no firmware-state writes.
local machine=manager.machine
assert(machine.system.name=='npe3ap5')
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local handles,busy={},false
for _,entry in ipairs({0x34b4,0x350c,0x3500,0x45b2,0x45ba,0x45c2,0x8508}) do
    local hits=0
    handles[#handles+1]=cpu.spaces.program:install_read_tap(entry,entry,'npe3_dedicated_'..entry,function()
        -- Stop-time memory snapshots also read program space. They are not
        -- execution. Even live program fetches may be speculative, so the
        -- separate Write owner is authoritative for flag mutations.
        if busy or cpu.state.PC.value~=entry+1 or hits>=8 then return end
        busy=true
        hits=hits+1
        local fields={}
        for _,name in ipairs({'PC','EXECPC','A','B','ST0','ST1','AR0','AR2','AR3','AR4','AR5','AR6','BK'}) do
            if cpu.state[name] then fields[#fields+1]=string.format('%s=%x',name,cpu.state[name].value) end
        end
        local words={}
        for _,at in ipairs({0x4fa,0x866,0x4ef,0x4f0,0x4f1,0x4f2,0x4f3,0x4f4,0x2f00,0x2f01,0x2f02,0x2f03,0x2f04,0x2f05}) do
            words[#words+1]=string.format('%04x:%04x',at,cpu.spaces.data:read_u16(at))
        end
        local ar2=cpu.state.AR2.value&0xffff
        local ring={}
        for at=ar2,math.min(ar2+15,65535) do ring[#ring+1]=string.format('%04x',cpu.spaces.data:read_u16(at)) end
        machine:logerror(string.format('npe3_dedicated_entry: entry=%04x hit=%d t=%.9f %s cells=%s ring=%s observer_writes=0\n',
            entry,hits,machine.time:as_double(),table.concat(fields,' '),table.concat(words,','),table.concat(ring,',')))
        busy=false
    end)
end
local writes=0
local captured=false
local parks=0
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x866,0x866,'npe3_send_block_park',function(_,value,mask)
    if cpu.state.EXECPC.value~=0x3507 then return end
    parks=parks+1
    if parks<=8 then
        machine:logerror(string.format('npe3_send_block_park: sequence=%d t=%.9f value=%04x mask=%04x owner=003507 observer_writes=0\n',
            parks,machine.time:as_double(),value,mask))
    end
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_send_block_park_complete: writes=%d cap=8 observer_writes=0\n',parks))
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x4fa,0x4fa,'npe3_integrity_flags',function(_,value,mask)
    if writes>=32 then return end
    writes=writes+1
    machine:logerror(string.format('npe3_integrity_flags: sequence=%d t=%.9f value=%04x mask=%04x owner=%06x observer_writes=0\n',
        writes,machine.time:as_double(),value,mask,cpu.state.EXECPC.value))
    if not captured and cpu.state.EXECPC.value==0x45b4 then
        captured=true
        busy=true
        local file=assert(io.open('npe3_record_program.be16','wb'))
        for at=0,65535 do file:write(string.pack('>I2',cpu.spaces.program:read_u16(at))) end
        file:close()
        busy=false
    end
end)
_G.npe3_dedicated_observation=handles
local arm=assert(machine.devices[':maincpu'])
local space=arm.spaces.program
local record_reads=0
handles[#handles+1]=space:install_read_tap(0x503258,0x50325b,'npe3_record_storage_read',function()
    if busy or arm.state.PC.value~=0x503258 or machine.time:as_double()>2 or
            arm.state.R2.value~=24 or arm.state.R0.value<0x20 or arm.state.R0.value>=0x110 then return end
    record_reads=record_reads+1
    if record_reads<=16 then
        machine:logerror(string.format('npe3_record_storage_read: sequence=%d offset=%04x length=24 caller=%08x t=%.9f observer_writes=0\n',
            record_reads,arm.state.R0.value,arm.state.R14.value,machine.time:as_double()))
    end
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_record_storage_read_complete: reads=%d cap=16 observer_writes=0\n',record_reads))
end)
local flag_writes=0
handles[#handles+1]=space:install_write_tap(0x17fd98,0x17fd9b,'npe3_record_host_flags',function(address,value,mask)
    if address~=0x17fd98 or busy or flag_writes>=64 then return end
    busy=true
    local old=space:read_u32(address)
    local after=(old&~mask)|(value&mask)
    if ((old~after)&0x00ff0000)~=0 then
        flag_writes=flag_writes+1
        machine:logerror(string.format('npe3_record_host_flags: t=%.9f old=%02x new=%02x owner=%08x observer_writes=0\n',
            machine.time:as_double(),(old>>16)&255,(after>>16)&255,arm.state.PC.value))
        if arm.state.PC.value==0x3028dc then
            local faults={}
            for at=0x17fbe0,0x17fbf7 do faults[#faults+1]=string.format('%02x',space:read_u8(at)) end
            machine:logerror(string.format('npe3_record_host_faults: index=%u pointer=%08x bytes=%s t=%.9f observer_writes=0\n',
                arm.state.R0.value,arm.state.R1.value,table.concat(faults),machine.time:as_double()))
        end
    end
    busy=false
end)
for _,entry in ipairs({0x387ad4,0x387c78,0x387d7e,0x387e9e,0x387f66}) do
    local hits=0
    handles[#handles+1]=space:install_read_tap(entry&~3,(entry&~3)+3,'npe3_record_host_'..entry,function()
        if busy or arm.state.PC.value~=entry or hits>=4 then return end
        hits=hits+1
        machine:logerror(string.format('npe3_record_host: entry=%08x t=%.9f r0=%08x r4=%08x r5=%08x r6=%08x r7=%08x r8=%08x observer_writes=0\n',
            entry,machine.time:as_double(),arm.state.R0.value,arm.state.R4.value,arm.state.R5.value,
            arm.state.R6.value,arm.state.R7.value,arm.state.R8.value))
        if entry==0x387ad4 then
            local context={}
            busy=true
            for at=0x16bf8c,0x16bfab do context[#context+1]=string.format('%02x',space:read_u8(at)) end
            machine:logerror(string.format('npe3_record_context: hit=%d t=%.9f bytes=%s observer_writes=0\n',
                hits,machine.time:as_double(),table.concat(context)))
            busy=false
        end
    end)
end
