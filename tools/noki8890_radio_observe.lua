-- NSB-6 radio command construction, observation only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_security_input.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:bpset(0x2aed52, nil,
    'logerror "8890_scan_construct: mode=%02x parameter_index=%02x caller=%08x\\n",r0,r1,r14;g')
cpu.debug:bpset(0x2aed84, nil,
    'logerror "8890_scan_payload: message=%08x wire=%08x\\n",r4,d@(r4+4);g')
cpu.debug:bpset(0x21ef4c, nil,
    'logerror "8890_scan_owner: state=1 caller=%08x\\n",r14;g')
cpu.debug:bpset(0x2809fc, nil,
    'logerror "8890_scan_results: mode=%02x message=%08x caller=%08x\\n",r0,r1,r14;g')
cpu.debug:bpset(0x280a70, nil,
    'logerror "8890_scan_record: arfcn=%04x rssi=%08x mode=%02x\\n",r7,r11,r9;g')
cpu.debug:bpset(0x280afc, nil,
    'logerror "8890_scan_complete: state=%08x gsm=%04x dcs=%04x pcs=%04x reports=%02x\\n",d@r4,w@(r4+4),w@(r4+6),w@(r4+8),b@(r4+0xa);g')
