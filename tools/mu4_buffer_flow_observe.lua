-- Passive original-controller descriptor flow, not retained instruction ownership.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'buffer flow requires the original MU4 bench')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, nodes = {}, {}
local writes, observed, capped, closed = 0, 0, false, false
local previous = {}
local limit = 131072
local function log_write(kind, base, address, value, mask)
    observed = observed+1
    if mask == 0xffff and previous[address] == value then return end
    previous[address] = mask == 0xffff and value or nil
    writes = writes + 1
    if writes <= limit then
        machine:logerror(string.format(
            'mu4_buffer_write: ordinal=%u bus_ordinal=%u kind=%s base=%04x address=%04x value=%04x mask=%04x t=%.9f context_pc=%06x observer_writes=0\n',
            writes,observed,kind,base,address,value,mask,machine.time:as_double(),cpu.state.GENPC.value))
    else
        capped = true
    end
end
for _, address in ipairs({0x15e0,0x15df,0x12fb,0x12fc,0x1310}) do
    handles[#handles+1] = data:install_write_tap(address,address,'mu4_buffer_control_'..address,
        function(offset,value,mask) log_write('control',0,offset,value,mask) end)
end
machine:logerror('mu4_buffer_scope: descriptor_first=0.825 last=10.0 max_nodes=12 max_writes=131072 boundary=changed_data_write identical_full_writes=deduplicated context_pc=not_owner observer_writes=0\n')
local function discover(pointer)
    if pointer == 0 or nodes[pointer] then return end
    assert(pointer >= 0x80 and pointer <= 0xfff4,'invalid descriptor pointer')
    local count = 0
    for _ in pairs(nodes) do count = count+1 end
    assert(count < 12,'descriptor discovery capped')
    nodes[pointer] = true
    local values = {}
    for field = 0,11 do values[#values+1] = string.format('%04x',data:read_u16(pointer+field)) end
    machine:logerror(string.format('mu4_buffer_node: base=%04x words=%s t=%.9f observer_writes=0\n',
        pointer,table.concat(values,','),machine.time:as_double()))
    handles[#handles+1] = data:install_write_tap(pointer,pointer+11,'mu4_buffer_node_'..pointer,
        function(address,value,mask) log_write('descriptor',pointer,address,value,mask) end)
end
local frame = emu.register_frame_done(function()
    if closed then return end
    if machine.time:as_double() >= 10 then
        for _,handle in ipairs(handles) do handle:remove() end
        closed = true
        machine:logerror(string.format('mu4_buffer_closed: writes=%u bus_writes=%u capped=%u observer_writes=0\n',writes,observed,capped and 1 or 0))
        return
    end
    -- Before AA99 initialization these addresses contain uploaded program words.
    if machine.time:as_double() < 0.825 then return end
    for _,root in ipairs({0x15e7,0x15e8,0x15e9}) do discover(data:read_u16(root)) end
    local linked = {}
    for pointer in pairs(nodes) do
        linked[#linked+1] = data:read_u16(pointer+8)
        linked[#linked+1] = data:read_u16(pointer+9)
    end
    for _,pointer in ipairs(linked) do discover(pointer) end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_buffer_terminal: closed=%u writes=%u bus_writes=%u capped=%u illegal=%u observer_writes=0\n',
        closed and 1 or 0,writes,observed,capped and 1 or 0,cpu.state.ILLEGAL.value))
end)
_G.mu4_buffer_flow_handles = {handles,frame,stop}
