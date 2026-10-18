-- Passive integration frontier: read ordinary uploaded RAM, never I/O/status.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local captured = false
emu.register_frame_done(function()
    if captured or machine.time:as_double() < 0.5 then return end
    captured = true
    local words = {}
    for address = 0x0f00, 0x0f27 do
        words[#words + 1] = string.format('%04x', cpu.spaces.program:read_u16(address))
    end
    machine:logerror(string.format(
        'rom4_staged_frontier: pc=%04x xpc=%02x illegal=%d pmst=%04x swwsr=%04x bscr=%04x swcr=%04x words=%s t=%.9f\n',
        cpu.state.PC.value, cpu.state.XPC.value, cpu.state.ILLEGAL.value,
        cpu.state.PMST.value, cpu.state.SWWSR.value, cpu.state.BSCR.value,
        cpu.state.SWCR.value, table.concat(words, ','), machine.time:as_double()))
    machine:logerror(string.format(
        'rom4_staged_dagen: bk=%04x ar0=%04x ar2=%04x st0=%04x st1=%04x\n',
        cpu.state.BK.value, cpu.state.AR0.value, cpu.state.AR2.value,
        cpu.state.ST0.value, cpu.state.ST1.value))
end)
