-- Existing physical contact-save workflow; observe SIM bytes without guest writes.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_phonebook_fixture.lua')
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces.program
local retained = {}
_G.nse2_toolkit_wire_taps = retained
local function wire(address, data, mask, write)
    for lane = 0, 3 do
        local offset, shift = address + lane, (3 - lane) * 8
        if mask & (255 << shift) ~= 0 and
            ((write and offset == 0x20036) or (not write and offset == 0x20037)) then
            machine:logerror(string.format(
                'nse2_toolkit_wire: direction=%s data=%02x t=%.9f observer_writes=0\n',
                write and 'TX' or 'RX', (data >> shift) & 255, machine.time:as_double()))
        end
    end
end
retained[#retained + 1] = memory:install_write_tap(0x20034, 0x20037,
    'nse2_toolkit_tx', function(a, d, m) wire(a, d, m, true) end)
retained[#retained + 1] = memory:install_read_tap(0x20034, 0x20037,
    'nse2_toolkit_rx', function(a, d, m) wire(a, d, m, false) end)
