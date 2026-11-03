PYTHON ?= python


#---------------------------------------------------------------------------------
# OUTPUT_DIR is the directory where final published files will be placed
#---------------------------------------------------------------------------------
OUTPUT_DIR		:=	../nitrofiles

FILES		:= files files/cht

#---------------------------------------------------------------------------------
# Goals for Build
#---------------------------------------------------------------------------------
.PHONY:

all:	$(OUTPUT_DIR)/apfix.pck

$(OUTPUT_DIR)/apfix.pck: $(FILES)
	$(PYTHON) pack.py $^ -o $@
