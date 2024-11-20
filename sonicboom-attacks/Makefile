###############################################################################
# Makefile for boom-attacks
###########################
# Makes baremetal executables to run on BOOM
###############################################################################

# Folders
SRC:=src
INC:=inc
LNK:=link
OBJ:=obj
OBJ_SECURE:=obj_secure
BIN:=bin
BIN_SECURE:=bin_secure
DMP:=dump
DEP:=dep
## For Verilator
SRCRV:=src/verilator
OBJRV:=objrv
BINRV:=binrv
DMPRV:=dumprv

# Commands and flags
CC:=riscv64-unknown-linux-gnu-gcc
OBJDUMP:=riscv64-unknown-linux-gnu-objdump -S
LDFLAGS:=-static -lgcc
## For Verilator
CCRV:=riscv64-unknown-elf-gcc
OBJDUMPRV:=riscv64-unknown-elf-objdump -S
LDFLAGSRV:=-static -nostdlib -nostartfiles -lgcc

CFLAGS=-mcmodel=medany -l -std=gnu99 -O0 -g -fno-common -fno-builtin-printf -Wall -I$(INC) -Wno-unused-function -Wno-unused-variable
DEPFLAGS=-MT $@ -MMD -MP -MF $(DEP)/$*.d
RM=rm -rf

# Programs to compile
PROGRAMS= returnStackBuffer indirBranchMispred condBranchMispred
BINS=$(addprefix $(BIN)/,$(addsuffix .riscv,$(PROGRAMS)))
BINS_SECURE=$(addprefix $(BIN_SECURE)/,$(addsuffix .riscv,$(PROGRAMS)))
DUMPS=$(addprefix $(DMP)/,$(addsuffix .dump,$(PROGRAMS)))

## For Verilator
BINSRV=$(addprefix $(BINRV)/,$(addsuffix .riscvrv,$(PROGRAMS)))
DUMPSRV=$(addprefix $(DMPRV)/,$(addsuffix .dumprv,$(PROGRAMS)))


.PHONY: all allrv
all: pteditor ptedit.o context.o $(BINS) $(BINS_SECURE) $(DUMPS) 
bin: pteditor ptedit.o context.o $(BINS) $(BINS_SECURE)
bin_secure: $(BINS_SECURE)
dumps: $(DUMPS)

## For Verilator
allrv: $(BINSRV) $(DUMPSRV)
binrv: $(BINSRV)
dumpsrv: $(DUMPSRV)


pteditor: module/pteditor.c
	cd module && make
	
ptedit.o: ./ptedit/ptedit.c ./ptedit/ptedit.h
	 $(CC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o ./ptedit/$@

context.o: context/context.c context/context.h 
	 $(CC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o ./context/$@


	
	
# Build object files
$(OBJ)/%.o: $(SRC)/%.S
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.c 
	@mkdir -p $(OBJ)
	@mkdir -p $(DEP)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	
$(OBJ_SECURE)/%.o: $(SRC)/%.c 
	@mkdir -p $(OBJ_SECURE)
	@mkdir -p $(DEP)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@ -DSECURE

## For Verilator	
$(OBJRV)/%.o: $(SRCRV)/%.S
	@mkdir -p $(OBJRV)
	$(CCRV) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o $@

$(OBJRV)/%.o: $(SRCRV)/%.c
	@mkdir -p $(OBJRV)
	@mkdir -p $(DEP)
	$(CCRV) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Build executable
$(BIN)/%.riscv: $(OBJ)/%.o $(OBJ)/stack.o 
	@mkdir -p $(BIN)
	$(CC) $(LDFLAGS) $< $(OBJ)/stack.o -o $@

$(BIN_SECURE)/%.riscv: $(OBJ_SECURE)/%.o $(OBJ)/stack.o context.o ptedit.o
	@mkdir -p $(BIN)_secure
	$(CC) $(LDFLAGS) $< $(OBJ)/stack.o ptedit/ptedit.o context/context.o -o $@_secure -DSECURE

## For Verilator	
$(BINRV)/%.riscvrv: $(OBJRV)/%.o $(OBJRV)/crt.o $(OBJRV)/syscalls.o $(OBJRV)/stack.o $(LNK)/link.ld
	@mkdir -p $(BINRV)
	$(CCRV) -T $(LNK)/link.ld $(LDFLAGSRV) $< $(OBJRV)/crt.o $(OBJRV)/stack.o $(OBJRV)/syscalls.o -o $@	

# Build dump
$(DMP)/%.dump: $(BIN)/%.riscv
	@mkdir -p $(DMP)
	$(OBJDUMP) -D $< > $@

## For Verilator	
$(DMPRV)/%.dumprv: $(BINRV)/%.riscvrv
	@mkdir -p $(DMPRV)
	$(OBJDUMPRV) -D $< > $@
	

# Keep the temporary .o files
.PRECIOUS: $(OBJ)/%.o
.PRECIOUS: $(OBJRV)/%.o

# Remove all generated files
clean:
	cd module && make clean
	rm -rf $(BIN) $(BIN_SECURE) $(OBJ) $(OBJ_SECURE) $(DMP) $(DEP) $(BINRV) $(OBJRV) $(DMPRV)
	rm -rf context/context.o ptedit/ptedit.o
	

# Include dependencies
-include $(addprefix $(DEP)/,$(addsuffix .d,$(PROGRAMS)))
