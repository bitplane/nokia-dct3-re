-- Physical inputs only; no task events or firmware/MMIO writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'nsm1_native_observe.lua')
local machine = manager.machine
local sequence = {
    {3.0, ':COL.2', 'Keypad 1', 1}, {3.2, ':COL.2', 'Keypad 1', 0},
    {3.8, ':COL.1', 'Left Softkey', 1}, {4.0, ':COL.1', 'Left Softkey', 0},
    {4.5, ':COL.1', 'Up', 1}, {4.7, ':COL.1', 'Up', 0},
}
local next_event = 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #sequence and now >= sequence[next_event][1] do
        local event = sequence[next_event]
        assert(machine.ioport.ports[event[2]].fields[event[3]]):set_value(event[4])
        machine:logerror(string.format(
            'nsm1_keypad_input: name=%s pressed=%d t=%.9f\n', event[3], event[4], now))
        next_event = next_event + 1
    end
end, 'frame')
