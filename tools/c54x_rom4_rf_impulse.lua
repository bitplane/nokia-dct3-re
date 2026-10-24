-- Diagnostic external RF-port impulse; no firmware RAM/register writes.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
machine:logerror('rom4_rf_profile: system='..machine.system.name..'\n')
local staged=machine.system.name=='nse1pipe' or machine.system.name=='nse1plg'
local impulse = tonumber(os.getenv("ROM4_RF_IMPULSE_INDEX") or "0")
assert(impulse and impulse >= 0 and impulse < 32 and impulse % 1 == 0)
local start = tonumber(os.getenv("ROM4_RF_IMPULSE_TIME") or "1.6402")
assert(start and start >= 0)
local amplitude = tonumber(os.getenv("ROM4_RF_IMPULSE_VALUE") or "16384")
assert(amplitude and amplitude >= 0 and amplitude <= 0xffff and amplitude % 1 == 0)
local mixed = os.getenv('ROM4_RF_MIXED_WORDS') == '1'
local full_scale = os.getenv('ROM4_RF_MIXED_FULL_SCALE') == '1'
assert(not full_scale or mixed)
local extrema = {0, 0x7fff, 0x8000, 0xffff, 0x7ffe, 0x8001, 0x1234, 0xedcb}
local inputs, outputs = 0, 0
local taps = {}
taps[#taps + 1] = cpu.spaces["io"]:install_read_tap(0x27, 0x27,
    "external_rf_impulse", function()
        if machine.time:as_double() < start then return 0 end
        -- Clock/core changes can move the first eligible read into another
        -- consumer. Start at the filter's first IN, not a wall-time accident.
        local owner=staged and cpu.state['READPC'].value or cpu.state['PC'].value-2
        if inputs == 0 and owner ~= 0x330a then return 0 end
        local value = inputs == impulse and amplitude or 0
        if mixed then
            value = inputs < 32 and (((inputs * 7919 + 1237) % 4096 - 2048) & 0xffff) or 0
            if full_scale then value = inputs < 32 and extrema[inputs % #extrema + 1] or 0 end
        end
        if inputs < 32 then
            machine:logerror(string.format(
                "rom4_rf_impulse_input: index=%d pc=%04x value=%04x owner=%08x\n",
                inputs, cpu.state["PC"].value, value,owner))
        end
        inputs = inputs + 1
        return value
    end)
taps[#taps + 1] = cpu.spaces["data"]:install_write_tap(0x1a48, 0x1a57,
    "rf_filter_output", function(offset, value)
        local pc = cpu.state["PC"].value
        local owner=staged and cpu.state['EXECPC'].value or pc-1
        if inputs == 0 or owner < 0x330a or owner > 0x3325 then return end
        if outputs >= 48 then return end
        machine:logerror(string.format(
            "rom4_rf_impulse_output: index=%d pc=%04x address=%04x value=%04x owner=%08x\n",
            outputs, cpu.state["PC"].value, offset, value,owner))
        outputs = outputs + 1
    end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom4_rf_cpu_state: illegal=%d pc=%04x\n',
        cpu.state['ILLEGAL'].value,cpu.state['PC'].value))
    if mixed then
        machine:logerror(string.format(
            '%s: samples=32 inputs=%d outputs=%d acquisition_claim=0\n',
            full_scale and 'rom4_rf_mixed_full_scale_summary' or 'rom4_rf_mixed_summary', inputs, outputs))
        return
    end
    machine:logerror(string.format(
        "rom4_rf_impulse_summary: impulse=%d amplitude=%04x inputs=%d outputs=%d acquisition_claim=0\n",
        impulse, amplitude, inputs, outputs))
end)
_G.rom4_rf_impulse = {taps = taps, stop = stop}
