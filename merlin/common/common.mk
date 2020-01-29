
SW_DIR=../../../src/

KERNEL_DIR:=$(PWD)
#SW_CODE :=  $(SW_DIR)/main.c $(SW_DIR)/compress.c $(SW_DIR)/util_timer.c $(SW_DIR)/seq_match.c $(SW_DIR)/header.c \
#						$(SW_DIR)/entropy.c $(SW_DIR)/fse_compress.c $(SW_DIR)/hist.c $(SW_DIR)/entropy_compress.c  \
#						$(SW_DIR)/multiplexer.c $(SW_DIR)/pre_offset.c $(SW_DIR)/seq_merge.c $(SW_DIR)/seq_sel.c \
#						$(SW_DIR)/self_match.c $(SW_DIR)/self_match_alt.c

SW_CODE = $(SW_DIR)/com_img.c \
		$(SW_DIR)/com_ipred.c \
		$(SW_DIR)/com_recon.c \
		$(SW_DIR)/com_tbl.c \
		$(SW_DIR)/com_util.c \
		$(SW_DIR)/com_itdq.c \
		$(SW_DIR)/com_df.c \
		$(SW_DIR)/com_sao.c \
		$(SW_DIR)/com_ComAdaptiveLoopFilter.c \
		$(SW_DIR)/com_picman.c \
		$(SW_DIR)/dec.c \
		$(SW_DIR)/dec_eco.c \
		$(SW_DIR)/dec_util.c \
		$(SW_DIR)/dec_bsr.c \
		$(SW_DIR)/dec_DecAdaptiveLoopFilter.c \
		$(SW_DIR)/enc.c \
		$(SW_DIR)/enc_eco.c \
		$(SW_DIR)/enc_mode.c \
		$(SW_DIR)/enc_pintra.c \
		$(SW_DIR)/enc_tq.c \
		$(SW_DIR)/enc_util.c \
		$(SW_DIR)/enc_tbl.c \
		$(SW_DIR)/enc_bsw.c \
		$(SW_DIR)/enc_EncAdaptiveLoopFilter.c \

# These files are excluded temporarily because it contains unsupported SSE instruction calls
#		$(SW_DIR)/enc_pinter.c \
#		$(SW_DIR)/com_mc.c \
#		$(SW_DIR)/enc_sad.c \

CSRCS_APP =	$(DIR_APP)/app_encoder.c \
			$(DIR_APP)/app_decoder.c \
			$(DIR_APP)/app_bitstream_merge.c


MACRO_CODE := $(patsubst $(SW_DIR)/%.c,separate/%.m,$(SW_CODE)) 
SEP_CODE   := $(patsubst $(SW_DIR)/%.c,./%.c,$(SW_CODE)) 

INCL_FLAGS   := -I../../common/ -I$(SW_DIR)/ -I$(SW_DIR)/../inc 
INCL_FLAGS_L := -I../../common/ -I./ -I./inc

CFLAGS := $(ADD_CFLAGS) $(CFLAGS)

kernel_separate:  kernel_clean cp_src
	make macro 
	-mv ../src/__merlinkernel.c ../src/__merlinkernel.c.bak
	cd separate; echo "cd separate; time mars_opt -e c -p kernel_separate -a kernel_name_suffix="_m" -a simple=on $(CFLAGS) $(SEP_CODE) $(INCL_FLAGS_L) " |& tee mars_opt.log
	cd separate;                    time mars_opt -e c -p kernel_separate -a kernel_name_suffix="_m" -a simple=on $(CFLAGS) $(SEP_CODE) $(INCL_FLAGS_L)   |& tee -a mars_opt.log
	ls src >& /dev/null || mkdir src
	cd separate; cp __merlinkernel_*.c ../src/__merlinkernel.c; cp merlin_type_define.h ../src/
	make kernel_lib

kernel_lib:
	cd src; gcc -c $(CFLAGS) __merlinkernel.c -fPIC -static -o libkernel.so -std=c99 $(INCL_FLAGS_L)

test: kernel_lib
	make test -C ../../run/ ADD_CFLAGS="-I$(KERNEL_DIR)/../common -DMCC_ACC $(ADD_CFLAGS) -L$(KERNEL_DIR)/src -lkernel -Wno-error=unknown-pragmas"

test_clean: 
	make clean -C ../../run/ ADD_CFLAGS="-I$(KERNEL_DIR)/../common -DMCC_ACC $(ADD_CFLAGS) -L$(KERNEL_DIR)/src -lkernel -Wno-error=unknown-pragmas"

debug :  
	cd separate; gdb --args mars_opt_org -e c -p kernel_separate -a kernel_name_suffix="_m" -a simple=on $(CFLAGS) $(SEP_CODE) $(INCL_FLAGS_L) 

cp_src: 
	cd separate; cp -r $(SW_DIR)/* . ; cd -
	cd separate; cp -r $(SW_DIR)/../inc ./inc/ ; cd -

macro: $(MACRO_CODE)


separate/%.m : separate/%.c 
	cd separate; gcc -E -C ../$^ -o ../$@ $(CFLAGS) $(INCL_FLAGS_L)
	mv $@ $^

kernel_clean: 
	ls separate || mkdir separate/;
	rm -rf separate/* ; rm -rf separate/.f*
	rm -rf estimate/* ; rm -rf estimate/.M*; rm -rf estimate/.m*

clean:kernel_clean

kernel_estimate: separate/__merlinkernel_*.c
	cd estimate; time merlincc -d11 --report=estimate ../$^
