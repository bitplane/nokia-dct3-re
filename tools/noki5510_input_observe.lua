-- Observe NPM-5 input ownership without installing another product's key map.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki5510_bootstrap_observe.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
assert(cpu.state['R14'], 'ARM debugger expressions require r14, not lr')
for index = 0, 29 do
    local entry = cpu.spaces['program']:read_u32(0x419d20 + index * 12) & ~1
    cpu.debug:bpset(entry, nil,
        string.format('logerror "5510_task_enter: index=%02x entry=%08x\\n";g', index, entry))
end
for _, tap in ipairs({
    {0x3a7424, 'gpio_init'},
    {0x3a738c, 'gpio_read'},
    {0x3a73d8, 'gpio_irq'},
    {0x39dc14, 'key_decode'},
    {0x335624, 'secondary_irq_post'},
    {0x33565a, 'serial_ui_init'},
    {0x335854, 'serial_ui_dispatch'},
    {0x335dac, 'serial_ui_probe'},
}) do
    cpu.debug:bpset(tap[1], nil,
        string.format('logerror "5510_input_%s: mode=%%02x caller=%%08x\\n",b@126ec6,r14;g', tap[2]))
end
cpu.debug:bpset(0x335cfa, nil,
    'logerror "5510_input_serial_ui_receive: message=%08x length=%04x header=%02x,%02x,%02x,%02x,%02x,%02x,%02x payload=%02x,%02x,%02x\\n",r0,w@(r0+4),b@r0,b@(r0+1),b@(r0+2),b@(r0+3),b@(r0+6),b@(r0+7),b@(r0+8),b@(r0+9),b@(r0+a),b@(r0+b);g')
cpu.debug:bpset(0x31475c, 'temp6<16',
    'temp6=temp6+1;logerror "5510_input_readiness_fields: outer=%02x selector=%02x nested=%02x final=%02x\\n",b@124880,b@119510,b@11ac28,b@11ac2a;g')
for _, address in ipairs({0x124880, 0x119510, 0x11ac28, 0x11ac2a}) do
    cpu.debug:wpset(cpu.spaces['program'], 'w', address, 1, 'temp7<80',
        'temp7=temp7+1;logerror "5510_input_readiness_write: pc=%08x address=%08x data=%02x\\n",pc,wpaddr,wpdata;g')
end
cpu.debug:bpset(0x2cebdc, nil,
    'logerror "5510_input_tasks_created: task29_state=%02x stack=%08x\\n",b@10d4e5,d@10d4e0;g')
cpu.debug:bpset(0x37bf06, nil,
    'logerror "5510_input_startup_gate: ready=%02x busy=%02x,%02x,%02x,%02x fiqmask=%02x\\n",r0,b@120c70,b@120c74,b@11cddc,b@11cdd8,b@2000a;g')
for _, tap in ipairs({
    {0x37bf0e, '3139a4'}, {0x37bf16, '31475c'},
    {0x37bf1e, '36039c'}, {0x37bf26, '350b42'},
    {0x37bf2e, '2c7302'}, {0x37bf36, '350c2c'},
}) do
    cpu.debug:bpset(tap[1], 'temp9<80',
        string.format('temp9=temp9+1;logerror "5510_input_readiness: predicate=%s result=%%02x\\n",r0;g', tap[2]))
end
for _, address in ipairs({0x120c70, 0x11cddc}) do
    cpu.debug:wpset(cpu.spaces['program'], 'w', address, 1, nil,
        'logerror "5510_input_busy_write: pc=%08x address=%08x data=%02x\\n",pc,wpaddr,wpdata;g')
end
