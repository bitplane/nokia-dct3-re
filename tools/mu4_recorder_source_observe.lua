-- R060 recorder bench only. Passive guest-access taps; no CPU/memory writes.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'recorder source observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles = {}
local field_writes, field_reads, nonzero_writes = 0, 0, 0
local first_copy_reads, order_errors = 0, 0
local first_copy_before_nonzero = false
local selector_reads = 0

local function context()
    return cpu.state['GENPC'].value
end

handles[1] = data:install_write_tap(0xfe78, 0xfe78, 'mu4_recorder_source_field_write', function(_, value, mask)
    field_writes = field_writes + 1
    if value ~= 0 then nonzero_writes = nonzero_writes + 1 end
    if field_writes <= 4 then
        machine:logerror(string.format('mu4_recorder_source_field_write: value=%04x mask=%04x context_pc=%06x t=%.9f\n', value, mask, context(), machine.time:as_double()))
    end
end)
handles[2] = data:install_read_tap(0xfe78, 0xfe78, 'mu4_recorder_source_field_read', function(_, value)
    field_reads = field_reads + 1
    if field_reads <= 4 then
        machine:logerror(string.format('mu4_recorder_source_field_read: value=%04x context_pc=%06x t=%.9f\n', value, context(), machine.time:as_double()))
    end
end)
handles[3] = data:install_read_tap(0x05b8, 0x05b8, 'mu4_recorder_source_selector', function(_, value)
    if selector_reads < 4 then
        machine:logerror(string.format('mu4_recorder_source_selector: value=%04x context_pc=%06x t=%.9f\n', value, context(), machine.time:as_double()))
    end
    selector_reads = selector_reads + 1
end)
handles[4] = data:install_read_tap(0, 0x7f, 'mu4_recorder_first_low_page_copy', function(address, value)
    -- Atomic MVDD executes at 03:8eda; its PC has advanced before data Read.
    -- The staged guard rejects this source before any such Read occurs.
    if cpu.state['XPC'].value ~= 3 or cpu.state['PC'].value ~= 0x8edb or first_copy_reads >= 128 then return end
    if first_copy_reads == 0 then first_copy_before_nonzero = nonzero_writes == 0 end
    if address ~= first_copy_reads then order_errors = order_errors + 1 end
    first_copy_reads = first_copy_reads + 1
    if address == 0 or address == 0x20 or address == 0x7f then
        machine:logerror(string.format('mu4_recorder_low_page_read: address=%04x value=%04x ordinal=%u t=%.9f\n', address, value, first_copy_reads, machine.time:as_double()))
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_recorder_source_summary: field_writes=%u field_reads=%u nonzero_writes=%u first_copy_reads=%u order_errors=%u first_copy_before_nonzero=%u selector_reads=%u observer_writes=0\n',
        field_writes, field_reads, nonzero_writes, first_copy_reads, order_errors, first_copy_before_nonzero and 1 or 0, selector_reads))
end)
_G.mu4_recorder_source_handles = {handles, stop}
