-- Actual data-bus reads for packet binding; no guest writes or audio claim.
local machine = manager.machine
assert(machine.system.name == 'mu4nand')
local cpu = assert(machine.devices[':cpu'])
local last = _G.mu4_packet_window_last or 10.0
assert(last == 10.0 or last == 12.0,'unsupported bounded packet window')
local reads, closed = 0, false
machine:logerror(string.format('mu4_packet_read_scope: first=0.8 last=%.1f first_address=162e last_address=1e2d cap=131072 ownership=data_bus pc=context_only observer_writes=0 audio_acceptance=0\n',last))
local tap = cpu.spaces.data:install_read_tap(0x162e,0x1e2d,'mu4_packet_reads',function(address,value,mask)
    if closed or machine:side_effects_disabled() then return end
    local time, pc = machine.time:as_double(), cpu.state.GENPC.value
    if time < 0.8 or time > last or pc < 0x2510c or pc > 0x25143 then return end
    reads = reads + 1
    if reads <= 131072 then
        machine:logerror(string.format('mu4_packet_read: ordinal=%u address=%04x word=%04x mask=%04x t=%.9f context_pc=%06x observer_writes=0\n',reads,address,value,mask,time,pc))
    end
end)
local frame = emu.register_frame_done(function()
    if not closed and machine.time:as_double() > last then
        tap:remove()
        closed = true
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_packet_read_terminal: reads=%u closed=%u t=%.9f illegal=%u observer_writes=0 audio_acceptance=0\n',reads,closed and 1 or 0,machine.time:as_double(),cpu.state.ILLEGAL.value))
end)
_G.mu4_packet_read_handles={tap,frame,stop}
