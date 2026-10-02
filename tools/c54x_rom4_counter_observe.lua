-- Read-only observation of the declared CTSI counter clock during ROM4 boot.
-- This checks model consistency, not the fitted silicon's physical frequency.
local machine = manager.machine
local dsp = assert(machine.devices[":dsp_c54x:cpu"])
local io = dsp.spaces["io"]
local taps = {}
local period, previous, checked, errors, advances, reads = 5000, nil, 0, 0, 0, 0

taps[#taps + 1] = io:install_write_tap(0x0d, 0x0e, "ctsi_counter_write",
    function(offset, data)
        if offset == 0x0e then period = (data == 0 and 4999 or data) + 1 end
        previous = nil
    end)
local function observe(data)
        reads = reads + 1
        if checked >= 4096 then return end
        local now = machine.time:as_double()
        if previous then
            local elapsed = (now - previous.time) * 13000000 / 12
            -- Do not join long gaps across boot/reset phases.
            if elapsed >= 0 and elapsed < period then
                local delta = (data - previous.value) % period
                local error_ticks = (delta - elapsed + period / 2) % period - period / 2
                checked = checked + 1
                if math.abs(error_ticks) > 1.01 then errors = errors + 1 end
                if delta ~= 0 then advances = advances + 1 end
                if checked <= 32 or math.abs(error_ticks) > 1.01 then
                    machine:logerror(string.format(
                        "rom4_counter_observe: pair=%d old=%04x value=%04x elapsed_ticks=%.6f error_ticks=%.6f pc=%04x\n",
                        checked, previous.value, data, elapsed, error_ticks, dsp.state["PC"].value))
                end
            end
        end
        previous = {time = now, value = data}
end
taps[#taps + 1] = io:install_read_tap(0x0d, 0x0d, "ctsi_counter_read",
    function(offset, data) observe(data) end)

-- Firmware reads this port only five times in the short boot. Debug reads
-- sample its non-destructive counter register without changing any state.
local sampler = coroutine.create(function()
    if not emu.wait(0.25) then return end
    previous = nil
    for _ = 1, 128 do
        local before = reads
        local value = io:read_u16(0x0d)
        if reads == before then observe(value) end
        if not emu.wait(0.0001) then return end
    end
end)
assert(coroutine.resume(sampler))

local stop_subscription = emu.add_machine_stop_notifier(function()
    local passed = checked >= 16 and advances > 0 and errors == 0
    print(string.format("ROM4 observed counter clock: %s reads=%d pairs=%d advances=%d errors=%d",
        passed and "PASS" or "FAIL", reads, checked, advances, errors))
end)

assert(#taps == 2 and stop_subscription)
