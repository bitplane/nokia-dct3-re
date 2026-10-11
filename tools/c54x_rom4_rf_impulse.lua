-- Diagnostic external RF-port impulse; no firmware RAM/register writes.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local impulse = tonumber(os.getenv("ROM4_RF_IMPULSE_INDEX") or "0")
assert(impulse and impulse >= 0 and impulse < 32 and impulse % 1 == 0)
local start = tonumber(os.getenv("ROM4_RF_IMPULSE_TIME") or "1.6402")
assert(start and start >= 0)
local amplitude = tonumber(os.getenv("ROM4_RF_IMPULSE_VALUE") or "16384")
assert(amplitude and amplitude >= 0 and amplitude <= 0xffff and amplitude % 1 == 0)
local inputs, outputs = 0, 0
local taps = {}
taps[#taps + 1] = cpu.spaces["io"]:install_read_tap(0x27, 0x27,
    "external_rf_impulse", function()
        if machine.time:as_double() < start then return 0 end
        -- Clock/core changes can move the first eligible read into another
        -- consumer. Start at the filter's first IN, not a wall-time accident.
        if inputs == 0 and cpu.state["PC"].value ~= 0x330c then return 0 end
        local value = inputs == impulse and amplitude or 0
        if inputs < 32 then
            machine:logerror(string.format(
                "rom4_rf_impulse_input: index=%d pc=%04x value=%04x\n",
                inputs, cpu.state["PC"].value, value))
        end
        inputs = inputs + 1
        return value
    end)
taps[#taps + 1] = cpu.spaces["data"]:install_write_tap(0x1a48, 0x1a57,
    "rf_filter_output", function(offset, value)
        local pc = cpu.state["PC"].value
        if inputs == 0 or pc < 0x330a or pc > 0x3325 then return end
        if outputs >= 48 then return end
        machine:logerror(string.format(
            "rom4_rf_impulse_output: index=%d pc=%04x address=%04x value=%04x\n",
            outputs, cpu.state["PC"].value, offset, value))
        outputs = outputs + 1
    end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format(
        "rom4_rf_impulse_summary: impulse=%d amplitude=%04x inputs=%d outputs=%d acquisition_claim=0\n",
        impulse, amplitude, inputs, outputs))
end)
_G.rom4_rf_impulse = {taps = taps, stop = stop}
