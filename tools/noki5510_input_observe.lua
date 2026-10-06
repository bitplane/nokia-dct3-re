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
}) do
    cpu.debug:bpset(tap[1], nil,
        string.format('logerror "5510_input_%s: mode=%%02x lr=%%08x\\\\n",b@126ec6,lr;g', tap[2]))
end
