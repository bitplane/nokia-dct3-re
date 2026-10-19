-- Actual scalar operands and stores; no sample/register intervention.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'mu4_output_processing_observe.lua')
local machine,cpu = manager.machine,manager.machine.devices[':cpu']
local handles,last = {},{}
local count,closed,capped = 0,false,false
machine:logerror('mu4_scale_scope: first=1.06 last=1.10 cap=100000 read_context=029fea..029ff1 write_context=029fee boundary=data_bus observer_writes=0 audio_acceptance=0\n')
local function observe(kind,address,value,mask)
    local time,pc = machine.time:as_double(),cpu.state.GENPC.value
    if closed or machine:side_effects_disabled() or time < 1 or time > 1.10 then return end
    if kind == 'write' then last[address] = {word=value,mask=mask,time=time,pc=pc} end
    if time < 1.06 or (kind == 'read' and (pc < 0x29fea or pc > 0x29ff1)) or (kind == 'write' and pc ~= 0x29fee) then return end
    count = count+1
    if count > 100000 then capped = true; return end
    local origin = kind == 'read' and last[address] or nil
    local detail = ''
    if kind == 'read' then
        detail = origin and string.format(' last_write_word=%04x last_write_mask=%04x last_write_t=%.9f last_write_pc=%06x',origin.word,origin.mask,origin.time,origin.pc)
                 or ' last_write_word=unobserved'
    end
    machine:logerror(string.format('mu4_scale_bus: ordinal=%u kind=%s address=%04x word=%04x mask=%04x t=%.9f context_pc=%06x ar1=%04x ar2=%04x ar5=%04x st1=%04x pmst=%04x%s observer_writes=0\n',
        count,kind,address,value,mask,time,pc,cpu.state.AR1.value,cpu.state.AR2.value,cpu.state.AR5.value,cpu.state.ST1.value,cpu.state.PMST.value,detail))
end
handles[2] = cpu.spaces.data:install_write_tap(0x1000,0x137f,'mu4_scale_outputs',
    function(address,value,mask) observe('write',address,value,mask) end)
local frame = emu.register_frame_done(function()
    -- Avoid routing the unrelated bootstrap/file-transfer reads through Lua.
    if not closed and not handles[1] and machine.time:as_double() >= 1.0 then
        handles[1] = cpu.spaces.data:install_read_tap(0,0xffff,'mu4_scale_operands',
            function(address,value,mask) observe('read',address,value,mask) end)
    end
    if not closed and machine.time:as_double() > 1.10 then
        for _,handle in ipairs(handles) do handle:remove() end
        closed = true
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_scale_terminal: events=%u closed=%u capped=%u illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        count,closed and 1 or 0,capped and 1 or 0,cpu.state.ILLEGAL.value,machine.time:as_double()))
end)
_G.mu4_scale_handles = {handles,frame,stop}
