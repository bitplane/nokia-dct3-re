-- Physical contacts only; inherited host labels remain provisional for NSE-6.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'nse6_stage_observe.lua')
local machine = manager.machine
local sequence = {
    {8.0, ':COL.2', 'Keypad 1', 1}, {8.2, ':COL.2', 'Keypad 1', 0},
    {10.0, ':COL.1', 'Left Softkey', 1}, {10.2, ':COL.1', 'Left Softkey', 0},
    {12.0, ':COL.1', 'Up', 1}, {12.2, ':COL.1', 'Up', 0},
}
local next_event = 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #sequence and now >= sequence[next_event][1] do
        local event = sequence[next_event]
        assert(machine.ioport.ports[event[2]].fields[event[3]]):set_value(event[4])
        machine:logerror(string.format(
            'nse6_physical_input: column=%s label=%s pressed=%d t=%.9f\n',
            event[2], event[3], event[4], now))
        local memory = machine.devices[':maincpu'].spaces['program']
        machine:logerror(string.format(
            'nse6_input_irq: pending=%02x mask=%02x control=%02x column_mask=%02x row_signal=%02x row_direction=%02x columns=%02x t=%.9f\n',
            memory:read_u8(0x20009), memory:read_u8(0x2000b),
            memory:read_u8(0x2000c), memory:read_u8(0x20033),
            memory:read_u8(0x20031), memory:read_u8(0x2002f),
            memory:read_u8(0x20030), now))
        next_event = next_event + 1
    end
end, 'frame')
