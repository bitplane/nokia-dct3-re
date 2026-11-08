-- Own MCU paging admission, only during an explicit external-page run.
local machine=manager.machine
if not os.getenv('NPE3_NATIVE_PAGE_AT') then return end
local cpu=assert(machine.devices[':maincpu'])
local space=cpu.spaces.program
local begin=tonumber(os.getenv('NPE3_NATIVE_PAGE_AT'))
local handles,counts={},{}
local busy=false
-- Function entries, branch destinations or call-return blocks, not arbitrary
-- interior PCs. The frame/group decision is complete at457e6e.
for _,entry in ipairs({0x457db8,0x457dd0,0x457e2a,0x457e6e,0x457e80,0x457ebe,0x457eec,0x457a32,0x327b5e}) do
    counts[entry]=0
    handles[#handles+1]=space:install_read_tap(entry&~3,(entry&~3)+3,'npe3_paging_'..entry,function()
        local now=machine.time:as_double()
        if busy or now<begin or now>24 or cpu.state.PC.value~=entry then return end
        counts[entry]=counts[entry]+1
        if counts[entry]>32 then return end
        busy=true
        local pointer=(entry==0x457db8 or entry==0x457a32) and cpu.state.R0.value or cpu.state.R4.value
        local object={}
        if pointer>=0x100000 and pointer+37<=0x180000 then
            for at=pointer,pointer+36 do object[#object+1]=string.format('%02x',space:read_u8(at)) end
        end
        local stack={}
        for at=0x14,0x16 do stack[#stack+1]=string.format('%02x',space:read_u8(cpu.state.R13.value+at)) end
        machine:logerror(string.format('npe3_paging_admission: entry=%08x hit=%d t=%.9f r0=%08x r1=%08x r2=%08x r3=%08x r5=%08x r6=%08x r7=%08x caller=%08x controller=%08x control=%04x group=%s object=%s observer_writes=0\n',
            entry,counts[entry],now,cpu.state.R0.value,cpu.state.R1.value,cpu.state.R2.value,cpu.state.R3.value,
            cpu.state.R5.value,cpu.state.R6.value,cpu.state.R7.value,cpu.state.R14.value,
            space:read_u32(0x172518),space:read_u16(0x16b26c),table.concat(stack),table.concat(object)))
        busy=false
    end)
end
local posts=0
handles[#handles+1]=space:install_read_tap(0x3c22c0,0x3c22c3,'npe3_page_post',function()
    local now=machine.time:as_double()
    if busy or now<begin or now>24 or cpu.state.PC.value~=0x3c22c0 then return end
    local target=cpu.state.R0.value
    if target~=15 and target~=18 then return end
    local pointer=cpu.state.R1.value
    if pointer<0x100000 or pointer+40>0x180000 then return end
    busy=true
    local header=space:read_u16(pointer)
    local payload=header==0x5eb and space:read_u32(pointer+8) or 0
    busy=false
    if (target~=15 or header~=0x623) and (target~=18 or header~=0x5eb) then return end
    posts=posts+1
    if posts>8 then return end
    machine:logerror(string.format('npe3_paging_post: sequence=%d t=%.9f target=%u header=%04x object=%08x payload=%08x observer_writes=0\n',posts,now,target,header,pointer,payload))
end)
local parsed=0
handles[#handles+1]=space:install_read_tap(0x3cd154,0x3cd157,'npe3_page_parser_return',function()
    local now=machine.time:as_double()
    if busy or now<begin or now>24 or cpu.state.PC.value~=0x3cd154 then return end
    local pointer=cpu.state.R5.value
    if pointer<0x100000 or pointer+12>0x180000 then return end
    busy=true
    if space:read_u16(pointer)==0x5eb then
        parsed=parsed+1
        if parsed<=8 then
            machine:logerror(string.format('npe3_paging_parser_return: sequence=%d t=%.9f event=%04x message=%08x context=%08x observer_writes=0\n',
                parsed,now,cpu.state.R0.value,pointer,cpu.state.R4.value))
        end
    end
    busy=false
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    for entry,count in pairs(counts) do
        machine:logerror(string.format('npe3_paging_admission_complete: entry=%08x hits=%d cap=32 end=24 observer_writes=0\n',entry,count))
    end
    machine:logerror(string.format('npe3_paging_delivery_complete: posts=%d post_cap=8 parsed=%d parse_cap=8 end=24 observer_writes=0\n',posts,parsed))
end)
_G.npe3_paging_observer=handles
