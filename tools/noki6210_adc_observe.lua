-- Read-only NPE-3 ADC caller census; does not identify electrical units.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:bpset(0x504100, nil,
    'logerror "6210_adc_request: channel=%02x caller=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x5041a0, nil,
    'logerror "6210_adc_result: raw=%04x caller=%08x\\n",r0,d@(r13+10);g')
