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
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x4fa,0x4fa,'npe3_integrity_flags',function(_,value,mask)
    if writes>=32 then return end
    writes=writes+1
    machine:logerror(string.format('npe3_integrity_flags: sequence=%d t=%.9f value=%04x mask=%04x owner=%06x observer_writes=0\n',
        writes,machine.time:as_double(),value,mask,cpu.state.EXECPC.value))
end)
_G.npe3_dedicated_observation=handles
