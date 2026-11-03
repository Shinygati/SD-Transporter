PYTHON ?= python


#---------------------------------------------------------------------------------
# OUTPUT_DIR is the directory where final published files will be placed
#---------------------------------------------------------------------------------
OUTPUT_DIR		:=	../nitrofiles

FILES		:= files

#---------------------------------------------------------------------------------
# Goals for Build
#---------------------------------------------------------------------------------
.PHONY:

all:	$(OUTPUT_DIR)/asyncLoadSettings.pck

$(OUTPUT_DIR)/asyncLoadSettings.pck: $(FILES)
	$(PYTHON) pack.py $^ -o $@
