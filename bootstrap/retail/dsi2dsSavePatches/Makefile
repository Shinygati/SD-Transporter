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

all:	$(OUTPUT_DIR)/dsi2dsSavePatches.pck

$(OUTPUT_DIR)/dsi2dsSavePatches.pck: $(FILES)
	$(PYTHON) pack.py $^ -o $@
