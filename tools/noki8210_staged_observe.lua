-- Own NSM-3 bootstrap selection, observation only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'dct3_model_scout.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
assert(cpu.debug, '8210 staged observation requires the debugger')
cpu.debug:bpset(0x2cac80, nil,
    'logerror "8210_verifier_descriptor: pointer=%08x\\n",d@135808;g')
cpu.debug:bpset(0x2cadd0, nil,
    'logerror "8210_verifier_result: result0=%04x result1=%04x\\n",w@10000,w@10002;g')
