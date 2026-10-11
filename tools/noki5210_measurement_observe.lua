-- Passive branch-entry observations for the own NSM-5 measurement contract.
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local taps = {}
for _, address in ipairs({0x37019c, 0x2e336c, 0x2bd782}) do
    local count = 0
    taps[#taps + 1] = cpu.spaces['program']:install_read_tap(address & ~3,
      (address & ~3) + 3, '5210_measurement_entry', function(offset, data, mask)
        if cpu.state['R15'].value ~= address then return end
        count = count + 1
        if count <= 8 then
            machine:logerror(string.format(
                '5210_measurement_entry: pc=%08x r0=%08x r1=%08x t=%.9f\n',
                address, cpu.state['R0'].value, cpu.state['R1'].value,
                machine.time:as_double()))
        end
      end)
end
_G.noki5210_measurement_observers = taps
