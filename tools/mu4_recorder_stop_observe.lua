-- Changed RAM writes only; context PCs are not retained writer identities.
local machine = manager.machine
assert(machine.system.name == 'mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = cpu.spaces.data
local handles, previous = {}, {}
local events, writes, capped, closed = 0, 0, false, false
machine:logerror('mu4_recorder_stop_scope: first=0.8 last=15 cap=32768 boundary=changed_data_write identical_full_writes=deduplicated context_pc=not_owner observer_writes=0 audio_acceptance=0\n')
for _, range in ipairs({{0x05eb,0x05f3},{0x0080,0x008f},{0xfd85,0xfd88},{0x373e,0x373e},{0x3762,0x3763}}) do
    handles[#handles+1] = data:install_write_tap(range[1],range[2],'mu4_recorder_stop_'..range[1],
        function(address,value,mask)
            local time = machine.time:as_double()
            if closed or machine:side_effects_disabled() or time < 0.8 or time > 15 then return end
            writes = writes+1
            if mask == 0xffff and previous[address] == value then return end
            previous[address] = mask == 0xffff and value or nil
            events = events+1
            if events > 32768 then capped = true; return end
            machine:logerror(string.format('mu4_recorder_stop_write: ordinal=%u bus_ordinal=%u address=%04x value=%04x mask=%04x t=%.9f context_pc=%06x observer_writes=0\n',
                events,writes,address,value,mask,time,cpu.state.GENPC.value))
        end)
end
local frame = emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 15 then
        for _,handle in ipairs(handles) do handle:remove() end
        closed = true
    end
    if machine.time:as_double() > 15.1 then machine:exit() end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_recorder_stop_terminal: events=%u bus_writes=%u closed=%u capped=%u illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        events,writes,closed and 1 or 0,capped and 1 or 0,cpu.state.ILLEGAL.value,machine.time:as_double()))
end)
_G.mu4_recorder_stop_handles = {handles,frame,stop}
