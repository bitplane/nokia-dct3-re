# The first makefile is the acquired reference's own makefile.gcc.
decoder_observed: $(DOBJ) efr_state_observer.o
	$(CC) $(CFLAGS) -o $@ $(DOBJ) efr_state_observer.o \
		-Wl,--wrap=Decoder_12k2,--wrap=D_plsf_5,--wrap=d_gain_pitch,--wrap=d_gain_code

efr_state_observer.o: $(OBSERVER_SOURCE)
	$(CC) $(CFLAGS) -c $(OBSERVER_SOURCE) -o $@
