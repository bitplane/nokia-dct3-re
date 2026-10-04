-- Physical matrix-cell probe: own ROM index 6 maps to logical key 0x19.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_runtime_observe.lua")
local machine = manager.machine
local key = nil
for _, field in pairs(machine.ioport.ports[":COL.1"].fields) do
    if field.mask == 0x02 then key = field end
end
assert(key, "missing physical matrix cell column 1/row 1")
local phase = 0
local memory = machine.devices[":maincpu"].spaces["program"]
local function context(label)
    machine:logerror(string.format("6250_physical_context: phase=%s row=%02x direction=%02x columns_mask=%02x irq_mask=%02x irq_pending=%02x\n",
        label, memory:read_u8(0x20028), memory:read_u8(0x200a8), memory:read_u8(0x2006b),
        memory:read_u8(0x2000b), memory:read_u8(0x2000a)))
end
emu.register_periodic(function()
    local time = machine.time:as_double()
    if phase == 0 and time >= 6 then
        context("before_press")
        key:set_value(1)
        phase = 1
        machine:logerror("6250_physical_cell: column=1 row=1 pressed=1\n")
    elseif phase == 1 and time >= 6.15 then
        context("before_release")
        key:set_value(0)
        phase = 2
        machine:logerror("6250_physical_cell: column=1 row=1 pressed=0\n")
    elseif phase == 2 and time >= 16 then
        context("settled_press")
        key:set_value(1)
        phase = 3
        machine:logerror("6250_physical_cell: column=1 row=1 pressed=1 t=16\n")
    elseif phase == 3 and time >= 16.15 then
        key:set_value(0)
        phase = 4
        machine:logerror("6250_physical_cell: column=1 row=1 pressed=0 t=16.15\n")
    end
end)
