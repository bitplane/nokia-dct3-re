-- Observe NPM-5 input ownership without installing another product's key map.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki5510_bootstrap_observe.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
for _, tap in ipairs({
    {0x3a7424, 'gpio_init'},
    {0x3a738c, 'gpio_read'},
    {0x3a73d8, 'gpio_irq'},
    {0x39dc14, 'key_decode'},
    {0x335624, 'secondary_irq_post'},
    {0x335f1e, 'serial_ui_init'},
    {0x335cfa, 'serial_ui_receive'},
    {0x335854, 'serial_ui_dispatch'},
    {0x335dac, 'serial_ui_probe'},
}) do
    cpu.debug:bpset(tap[1], nil,
        string.format('logerror "5510_input_%s: mode=%%02x lr=%%08x\\n",b@126ec6,lr;g', tap[2]))
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
