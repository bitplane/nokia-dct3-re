-- Passive early-boot gate. Passing does not imply graphical boot.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local dsp_ready, ccont_ready = false, false
local ownership = { [0x100fc] = 0, [0x10100] = 0 }
local last_owner, order_errors = nil, 0
local taps = {}
taps[#taps + 1] = memory:install_write_tap(0x20000, 0x20003,
    "6250_dsp_control", function(offset, value, mask)
        if (mask & 0xff00) ~= 0 then
            machine:logerror(string.format(
                "6250_dsp_control: data=%02x pc=%08x program0=%04x fields=%04x/%04x/%04x/%04x\n",
                (value >> 8) & 0xff, cpu.state["PC"].value,
                memory:read_u16(0x11e00), memory:read_u16(0x100f6),
                memory:read_u16(0x100f8), memory:read_u16(0x100fa), memory:read_u16(0x100fc)))
        end
    end)
taps[#taps + 1] = memory:install_read_tap(0x20000, 0x20003,
    "6250_reset_ready", function(offset, value, mask)
        if (mask & 0xff00) ~= 0 and ((value >> 8) & 0x10) ~= 0 then
            dsp_ready = true
        end
    end)
taps[#taps + 1] = memory:install_read_tap(0x2006c, 0x2006f,
    "6250_ccont_ready", function(offset, value, mask)
        if (mask & 0xff0000) ~= 0 and ((value >> 16) & 4) ~= 0 then
            ccont_ready = true
        end
    end)
taps[#taps + 1] = memory:install_write_tap(0x100fc, 0x10103,
    "6250_bootstrap_ownership", function(offset, value, mask)
        local shift = offset == 0x10100 and 16 or 0
        if ownership[offset] and ((mask >> shift) & 0xffff) ~= 0
                and ((value >> shift) & 0xffff) == 0 then
            ownership[offset] = ownership[offset] + 1
            if last_owner == offset then order_errors = order_errors + 1 end
            last_owner = offset
        end
    end)
local checked = false
local captured = false
emu.register_periodic(function()
    if not captured and machine.time:as_double() >= 0.5 then
        captured = true
        local program, fields = {}, {}
        for index = 0, 222 do
            program[#program + 1] = string.format("%04x", memory:read_u16(0x11e00 + index * 2))
        end
        for index = 0, 6 do
            fields[#fields + 1] = string.format("%04x", memory:read_u16(0x100f6 + index * 2))
        end
        machine:logerror(string.format("6250_verifier_capture: fields=%s program=%s\n",
            table.concat(fields, "/"), table.concat(program)))
    end
    if checked or machine.time:as_double() < 8 then return end
    checked = true
    machine:logerror(string.format(
        "6250_bootstrap_observation: dsp_ready=%d ccont_ready=%d transfers=%d/%d order_errors=%d pc=%08x t=%.3f\n",
        dsp_ready and 1 or 0, ccont_ready and 1 or 0,
        ownership[0x100fc], ownership[0x10100], order_errors, cpu.state["PC"].value,
        machine.time:as_double()))
    assert(dsp_ready, "6250 DSP ready was not observed on the live bus")
    assert(ccont_ready, "6250 CCONT receive-ready was not observed on the live bus")
    assert(ownership[0x100fc] == 116 and ownership[0x10100] == 116 and order_errors == 0,
        "6250 ownership sequence differs from the reviewed cold boundary")
    machine.screens[":screen"]:snapshot("6250_frontier.png")
    print("6250 early hardware gate: PASS (not graphical boot)")
end)
_G.noki6250_bootstrap_taps = taps
