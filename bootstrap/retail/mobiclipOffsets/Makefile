PYTHON ?= python


#---------------------------------------------------------------------------------
# OUTPUT_DIR is the directory where final published files will be placed
#---------------------------------------------------------------------------------
OUTPUT_DIR		:=	../nitrofiles

FILES			:= files

#---------------------------------------------------------------------------------
# Goals for Build
#---------------------------------------------------------------------------------
.PHONY:

all:	$(OUTPUT_DIR)/mobiclipOffsets.pck

$(OUTPUT_DIR)/mobiclipOffsets.pck: $(FILES)
	$(PYTHON) pack.py $^ -o $@
