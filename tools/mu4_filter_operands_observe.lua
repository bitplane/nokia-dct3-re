-- Bounded actual helper traffic; register/PC values are context, not ownership.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'mu4_output_scale_observe.lua')
local machine,cpu = manager.machine,manager.machine.devices[':cpu']
local handles = {}
local count,closed,capped = 0,false,false
machine:logerror('mu4_filter_scope: first=1.061 last=1.062 cap=50000 first_address=1000 last_address=13ff context=02a2c6..02a2eb boundary=data_bus observer_writes=0 audio_acceptance=0\n')
local function observe(kind,address,value,mask)
    local time,pc = machine.time:as_double(),cpu.state.GENPC.value
    if closed or machine:side_effects_disabled() or time < 1.061 or time > 1.062 or pc < 0x2a2c6 or pc > 0x2a2eb then return end
    count = count+1
    if count > 50000 then capped = true; return end
    machine:logerror(string.format('mu4_filter_bus: ordinal=%u kind=%s address=%04x word=%04x mask=%04x t=%.9f context_pc=%06x st1=%04x pmst=%04x observer_writes=0\n',
        count,kind,address,value,mask,time,pc,cpu.state.ST1.value,cpu.state.PMST.value))
end
local frame = emu.register_frame_done(function()
    local time = machine.time:as_double()
    if not closed and not handles[1] and time >= 1.0 then
        handles[1] = cpu.spaces.data:install_read_tap(0x1000,0x13ff,'mu4_filter_operand_reads',
            function(address,value,mask) observe('read',address,value,mask) end)
        handles[2] = cpu.spaces.data:install_write_tap(0x1000,0x13ff,'mu4_filter_state_writes',
            function(address,value,mask) observe('write',address,value,mask) end)
    end
    if not closed and time > 1.062 then
        for _,handle in ipairs(handles) do handle:remove() end
        closed = true
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_filter_terminal: events=%u closed=%u capped=%u illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        count,closed and 1 or 0,capped and 1 or 0,cpu.state.ILLEGAL.value,machine.time:as_double()))
end)
_G.mu4_filter_handles = {handles,frame,stop}
