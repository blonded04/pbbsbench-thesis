include common/parallelDefs

BNCHMRK = range

CHECKFILES = $(BNCHMRK)Check.o

COMMON = 

INCLUDE = 

%.o : %.C $(COMMON)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BNCHMRK)Check : $(CHECKFILES)
	$(CC) -o $@ $(CHECKFILES) $(LFLAGS)

clean :
	rm -f $(BNCHMRK)Check *.o
