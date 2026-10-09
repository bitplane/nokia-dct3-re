-- Physical registration only; permits a normal process exit with active service.
_G.dct3_divert_sequences = {'*21*5551234#'}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'noki6250_divert_lifecycle_input.lua')
