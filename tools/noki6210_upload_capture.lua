-- Read only the three qualified owned intervals after the native boundary halt.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_staged:cpu'])
local observer = coroutine.create(function()
    assert(emu.wait(2))
    assert(cpu.state.PC.value == 0x2c75, 'not at the qualified missing-resident boundary')
    for _, range in ipairs({{'bootstrap', 0x590, 422},
                            {'loader2', 0xa00, 629}, {'fragment', 0xff80, 104}}) do
        local output = assert(io.open('npe3_' .. range[1] .. '.bin', 'wb'))
        for address = range[2], range[2] + range[3] - 1 do
            output:write(string.pack('>I2', cpu.spaces.program:read_u16(address)))
        end
        output:close()
        machine:logerror(string.format(
            'npe3_upload_capture: block=%s start=%04x words=%d pc=%04x\n',
            range[1], range[2], range[3], cpu.state.PC.value))
    end
end)
_G.npe3_upload_capture = observer
assert(coroutine.resume(observer))
