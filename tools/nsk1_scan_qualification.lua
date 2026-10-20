-- Passive extension of the cold native-radio fixture; run for 92 seconds.
local source = debug.getinfo(1,'S').source:sub(2)
_G.nsk1_radio_no_call = true
dofile(source:match('^(.*[/\\])') .. 'nsk1_radio_observe.lua')
local machine = manager.machine
local dsp = machine.devices[':dsp_c54x:cpu']
local count = 0
local program_dumped = false
local publisher_taps = {}
for _,address in ipairs({0x9404,0x8e1c,0x3f50,0x4810}) do
    local entries = 0
    publisher_taps[#publisher_taps+1] = dsp.spaces.program:install_read_tap(address,address,'publisher_'..address,function()
        if machine.time:as_double() < 2 or entries >= 4 or dsp.state['PC'].value ~= address+1 then return end
        entries = entries + 1
        machine:logerror(string.format('nsk1_publisher_entry: address=%04x index=%d ar4=%04x ar5=%04x serial06b6=%04x t=%.6f observer_writes=0\n',address,entries,dsp.state['AR4'].value,dsp.state['AR5'].value,dsp.spaces.data:read_u16(0x06b6),machine.time:as_double()))
    end)
end
_G.nsk1_publisher_taps = publisher_taps
-- Program-read callbacks expose PC one word past this instruction's address.
_G.nsk1_scan_metric_tap = dsp.spaces.program:install_read_tap(0x0ae6,0x0ae6,'scan_metric',function()
    if machine.time:as_double() < 39 or count >= 8 then return end
    count = count + 1
    machine:logerror(string.format('nsk1_scan_metric: index=%d pc=%04x a=%010x threshold=%04x t=%.6f observer_writes=0\n',count,dsp.state['PC'].value,dsp.state['A'].value,dsp.spaces.data:read_u16(0x219b),machine.time:as_double()))
end)
local next_sample = 65
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if not program_dumped and now >= 45 then
        program_dumped = true
        local bytes = {}
        for address=0,0xffff do
            local word = dsp.spaces.program:read_u16(address)
            bytes[#bytes+1] = string.char((word >> 8)&255,word&255)
        end
        local output = assert(io.open('native_program_45s.bin','wb'))
        output:write(table.concat(bytes))
        output:close()
        machine.debugger:command('dasm native_serial_publishers.dasm,8d00,800,1,:dsp_c54x:cpu')
        machine:logerror('nsk1_live_program_snapshot: words=65536 byte_order=big t=45 observer_writes=0\n')
    end
    if now < next_sample or next_sample > 90 then return end
    next_sample = next_sample + 5
    local data = dsp.spaces.data
    machine:logerror(string.format('nsk1_scan_late: t=%.6f counter_hi=%04x counter_lo=%04x qualified=%04x flags=%04x observer_writes=0\n',now,data:read_u16(0x1262),data:read_u16(0x1263),data:read_u16(0x1344),data:read_u16(0x1266)))
end)
