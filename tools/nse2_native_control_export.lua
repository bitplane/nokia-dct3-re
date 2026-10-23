-- Standalone read-only export: use -debug -debugger none -autoboot_delay 0.
-- Wait for the firmware upload: the low program overlay is zero at cold entry.
local machine = manager.machine
assert(machine.system.name == 'nse2r4v' or machine.system.name == 'nse2r4t')
assert(machine.devices[':dsp_c54x:cpu'], 'native DSP required')
local done = false
emu.register_frame_done(function()
    if done or machine.time:as_double() < 7 then return end
    done = true
    local debugger = assert(machine.debugger, 'enable MAME debugger for control export')
    for _, range in ipairs({{'modes',0x4414,0xc0}, {'request',0x4dce,0x100},
                            {'measurement',0x926,0x15a}, {'filter',0x3380,0x90},
                            {'frame',0x3268,0x60}, {'wait',0x407c,0x70}}) do
        debugger:command(string.format('dasm nse2_control_%s.dasm,%x,%x,1,:dsp_c54x:cpu',
            range[1], range[2], range[3]))
    end
    machine:exit()
end, 'NSE-2 native control export after upload')
