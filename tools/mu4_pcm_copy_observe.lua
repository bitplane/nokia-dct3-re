-- Actual data-bus words; register/PC snapshots are context, not retained owners.
local machine = manager.machine
assert(machine.system.name == 'mu4nand')
local cpu = assert(machine.devices[':cpu'])
local events, reads, writes, capped, closed = 0, 0, 0, false, false
local handles, last_source_write, census = {}, {}, {}
local source_writes, destination_writes, program_alias_writes = 0, 0, 0
machine:logerror('mu4_pcm_copy_scope: first=1.0 last=1.25 cap=65536 source_first=7000 source_last=7fff destination_first=1000 destination_last=137f copy_objects=13e0,1460 read_context=02a31f..02a371 write_context=02a367 source_write_tracking=last_actual boundary=data_bus context_pc=not_owner observer_writes=0 audio_acceptance=0\n')
local function observe(kind,address,value,mask)
    local time,pc = machine.time:as_double(),cpu.state.GENPC.value
    if closed or machine:side_effects_disabled() or time < 1 or time > 1.25 then return end
    local object = cpu.state.AR4.value
    if object ~= 0x13e0 and object ~= 0x1460 then return end
    -- Final pipeline reads have already advanced GENPC beyond the MVDD word.
    if kind == 'read' and (pc < 0x2a31f or pc > 0x2a371) then return end
    if kind == 'write' and pc ~= 0x2a367 then return end
    events = events+1
    if kind == 'read' then reads = reads+1 else writes = writes+1 end
    if events > 65536 then capped = true; return end
    local origin = last_source_write[address]
    local detail = ''
    if kind == 'read' then
        detail = origin and string.format(' last_write_word=%04x last_write_mask=%04x last_write_t=%.9f last_write_pc=%06x',origin.word,origin.mask,origin.time,origin.pc)
                 or ' last_write_word=unobserved'
    end
    machine:logerror(string.format('mu4_pcm_copy_bus: ordinal=%u kind=%s address=%04x word=%04x mask=%04x t=%.9f context_pc=%06x ar1=%04x ar2=%04x ar3=%04x ar4=%04x%s observer_writes=0\n',
        events,kind,address,value,mask,time,pc,cpu.state.AR1.value,cpu.state.AR2.value,cpu.state.AR3.value,object,detail))
end
handles[1] = cpu.spaces.data:install_read_tap(0x7000,0x7fff,'mu4_pcm_source_reads',
    function(address,value,mask) observe('read',address,value,mask) end)
handles[2] = cpu.spaces.data:install_write_tap(0x1000,0x137f,'mu4_pcm_ring_writes',
    function(address,value,mask)
        local time,pc = machine.time:as_double(),cpu.state.GENPC.value
        if closed or machine:side_effects_disabled() or time < 1 or time > 1.25 then return end
        destination_writes = destination_writes+1
        census[pc] = (census[pc] or 0)+1
        observe('write',address,value,mask)
    end)
handles[3] = cpu.spaces.data:install_write_tap(0x7000,0x7fff,'mu4_pcm_source_writers',
    function(address,value,mask)
        local time = machine.time:as_double()
        if closed or machine:side_effects_disabled() or time < 1 or time > 1.25 then return end
        source_writes = source_writes+1
        last_source_write[address] = {word=value,mask=mask,time=time,pc=cpu.state.GENPC.value}
    end)
-- The mapped program alias forwards through AS_DATA.write_word. Count it
-- separately without replacing last-writer metadata or double-counting stores.
handles[4] = cpu.spaces.program:install_write_tap(0x7000,0x7fff,'mu4_pcm_program_alias_writers',
    function(address,value,mask)
        local time = machine.time:as_double()
        if closed or machine:side_effects_disabled() or time < 1 or time > 1.25 then return end
        program_alias_writes = program_alias_writes+1
    end)
local frame = emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.25 then
        for _,handle in ipairs(handles) do handle:remove() end
        closed = true
    end
    if machine.time:as_double() > 2 then machine:exit() end
end)
local stop = emu.add_machine_stop_notifier(function()
    local contexts = {}
    for pc in pairs(census) do contexts[#contexts+1] = pc end
    table.sort(contexts)
    for _,pc in ipairs(contexts) do
        machine:logerror(string.format('mu4_pcm_destination_census: context_pc=%06x writes=%u observer_writes=0\n',pc,census[pc]))
    end
    machine:logerror(string.format('mu4_pcm_copy_terminal: events=%u reads=%u writes=%u source_writes=%u destination_writes=%u contexts=%u closed=%u capped=%u illegal=%u t=%.9f program_alias_writes=%u observer_writes=0 audio_acceptance=0\n',
        events,reads,writes,source_writes,destination_writes,#contexts,closed and 1 or 0,capped and 1 or 0,cpu.state.ILLEGAL.value,machine.time:as_double(),program_alias_writes))
end)
_G.mu4_pcm_copy_handles = {handles,frame,stop}
