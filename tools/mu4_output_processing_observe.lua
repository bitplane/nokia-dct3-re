-- Reuse the bounded copy proof; add actual sample bus traffic, not PC ownership.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'mu4_pcm_copy_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':cpu'])
local handles,last = {},{}
local count,closed,capped = 0,false,false
machine:logerror('mu4_processing_scope: first=1.06 last=1.10 cap=100000 read_context=02a2c6..02a2ea read_address=ar7 write_context=02a2ea boundary=data_bus observer_writes=0 audio_acceptance=0\n')
local function observe(kind,address,value,mask)
    local time,pc = machine.time:as_double(),cpu.state.GENPC.value
    if closed or machine:side_effects_disabled() or time < 1.0 or time > 1.10 then return end
    if kind == 'write' then
        last[address] = {word=value,mask=mask,time=time,pc=pc}
    end
    if time < 1.06 or (kind == 'read' and (pc < 0x2a2c6 or pc > 0x2a2ea or address ~= cpu.state.AR7.value)) or (kind == 'write' and pc ~= 0x2a2ea) then return end
    count = count+1
    if count > 100000 then capped = true; return end
    local origin = kind == 'read' and last[address] or nil
    local detail = ''
    if kind == 'read' then
        detail = origin and string.format(' last_write_word=%04x last_write_mask=%04x last_write_t=%.9f last_write_pc=%06x',origin.word,origin.mask,origin.time,origin.pc)
                 or ' last_write_word=unobserved'
    end
    machine:logerror(string.format('mu4_processing_bus: ordinal=%u kind=%s address=%04x word=%04x mask=%04x t=%.9f context_pc=%06x ar2=%04x ar3=%04x ar4=%04x ar5=%04x ar7=%04x%s observer_writes=0\n',
        count,kind,address,value,mask,time,pc,cpu.state.AR2.value,cpu.state.AR3.value,cpu.state.AR4.value,cpu.state.AR5.value,cpu.state.AR7.value,detail))
end
handles[1] = cpu.spaces.data:install_read_tap(0x1000,0x137f,'mu4_processing_samples',
    function(address,value,mask) observe('read',address,value,mask) end)
handles[2] = cpu.spaces.data:install_write_tap(0x1000,0x137f,'mu4_processing_samples_write',
    function(address,value,mask) observe('write',address,value,mask) end)
local frame = emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.10 then
        for _,handle in ipairs(handles) do handle:remove() end
        closed = true
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_processing_terminal: events=%u closed=%u capped=%u illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        count,closed and 1 or 0,capped and 1 or 0,cpu.state.ILLEGAL.value,machine.time:as_double()))
end)
_G.mu4_processing_handles = {handles,frame,stop}
