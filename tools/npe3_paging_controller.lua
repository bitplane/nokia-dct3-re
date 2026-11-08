-- Current own-ROM controller frontier. Read-only,explicit page runs only.
local machine=manager.machine
if not os.getenv('NPE3_NATIVE_PAGE_AT') then return end
local cpu=assert(machine.devices[':maincpu'])
local space=cpu.spaces.program
local handles,counts={},{}
local busy=false
local transitions=0
-- "procedure" is a trace label for byte+6,not a recovered semantic name.
for _,field in ipairs({{0x169bee,1,'procedure'},{0x169bf3,1,'pending'},{0x169c06,2,'selector'},{0x169c14,4,'continuation'}}) do
    local address,size,name=table.unpack(field)
    local base=address&~3
    handles[#handles+1]=space:install_write_tap(base,base+3,'npe3_controller_'..name,function(_,value,mask)
        if busy or machine.time:as_double()>24 then return end
        busy=true
        local shift=(4-size-(address-base))*8
        local bits=size==4 and 0xffffffff or ((1<<(size*8))-1)
        local affected=(mask>>shift)&bits
        local previous=(space:read_u32(base)>>shift)&bits
        local incoming=(previous&~affected)|((value>>shift)&affected)
        if affected~=0 and incoming~=previous then
            transitions=transitions+1
            if transitions<=256 then
                machine:logerror(string.format('npe3_controller_transition: sequence=%d field=%s t=%.9f old=%08x new=%08x pc=%08x lr=%08x r0=%08x r1=%08x r4=%08x saved=%08x observer_writes=0\n',
                    transitions,name,machine.time:as_double(),previous,incoming,cpu.state.PC.value,cpu.state.R14.value,
                    cpu.state.R0.value,cpu.state.R1.value,cpu.state.R4.value,space:read_u32(cpu.state.R13.value+8)))
            end
        end
        busy=false
    end)
end
for _,entry in ipairs({0x31543e,0x315450,0x315472,0x314e04,0x3135c2}) do
    counts[entry]=0
    handles[#handles+1]=space:install_read_tap(entry&~3,(entry&~3)+3,'npe3_page_wait_'..entry,function()
        local now=machine.time:as_double()
        if busy or now<12 or now>24 or cpu.state.PC.value~=entry then return end
        counts[entry]=counts[entry]+1
        if counts[entry]>64 then return end
        busy=true
        local pointer=cpu.state.R4.value
        local context={}
        if pointer>=0x100000 and pointer+48<=0x180000 then
            for at=pointer,pointer+47 do context[#context+1]=string.format('%02x',space:read_u8(at)) end
        end
        local peer=cpu.state.R7.value
        local value=peer>=0x100000 and peer+4<=0x180000 and space:read_u32(peer) or 0xffffffff
        machine:logerror(string.format('npe3_paging_wait: entry=%08x hit=%d t=%.9f r0=%08x r1=%08x r4=%08x r5=%08x r6=%08x r7=%08x r9=%08x saved=%08x peer=%08x context=%s observer_writes=0\n',
            entry,counts[entry],now,cpu.state.R0.value,cpu.state.R1.value,pointer,cpu.state.R5.value,
            cpu.state.R6.value,peer,cpu.state.R9.value,space:read_u32(cpu.state.R13.value+8),value,table.concat(context)))
        busy=false
    end)
end
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_controller_transition_complete: changes=%d cap=256 end=24 observer_writes=0\n',transitions))
    for entry,count in pairs(counts) do
        machine:logerror(string.format('npe3_paging_wait_complete: entry=%08x hits=%d cap=64 begin=12 end=24 observer_writes=0\n',entry,count))
    end
end)
_G.npe3_paging_controller=handles
