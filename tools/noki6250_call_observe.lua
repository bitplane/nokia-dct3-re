-- Physical call input; outgoing mode is harness policy, not driver config.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_runtime_observe.lua")
local machine = manager.machine
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[":COL." .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error("missing physical matrix cell")
end
local actions = {
    {20, cell(0, 2), 1}, {20.2, cell(0, 2), 0},
    {24, cell(0, 3), 1}, {24.2, cell(0, 3), 0}
}
if _G.noki6250_host_incoming then
    actions = {
        {38, cell(0, 2), 1}, {38.2, cell(0, 2), 0},
        {46, cell(0, 3), 1}, {46.2, cell(0, 3), 0}
    }
end
if os.getenv("NOKIA_DCT3_6250_OUTGOING") == "1" then
    actions = {
        {16, cell(2, 1), 1}, {16.15, cell(2, 1), 0},
        {16.4, cell(3, 1), 1}, {16.55, cell(3, 1), 0},
        {16.8, cell(4, 1), 1}, {16.95, cell(4, 1), 0},
        {20, cell(0, 2), 1}, {20.2, cell(0, 2), 0},
        {28, cell(0, 3), 1}, {28.2, cell(0, 3), 0}
    }
end
if _G.noki6250_call_hold then
    table.remove(actions)
    table.remove(actions)
end
local captures = {17, 21, 26, 32}
local host_ready = false
if _G.noki6250_host_incoming then captures = {37, 44, 54} end
local next_action, next_capture = 1, 1
emu.register_periodic(function()
    local time = machine.time:as_double()
    if _G.noki6250_host_incoming and not host_ready and time >= 32 then
        machine.screens[":screen"]:snapshot("6250_host_registered_idle.png")
        host_ready = true
    end
    local action = actions[next_action]
    if action and time >= action[1] then
        action[2]:set_value(action[3])
        machine:logerror(string.format("6250_call_input: step=%d pressed=%d t=%.6f\n",
            next_action, action[3], time))
        next_action = next_action + 1
    end
    if captures[next_capture] and time >= captures[next_capture] then
        machine.screens[":screen"]:snapshot("6250_call_" .. next_capture .. ".png")
        next_capture = next_capture + 1
    end
end)
