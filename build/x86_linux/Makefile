DIR_TOP = ../..
DIR_INC = $(DIR_TOP)/inc
DIR_SRC = $(DIR_TOP)/src
DIR_APP = $(DIR_TOP)/app
DIR_LIB = $(DIR_TOP)/lib
DIR_BIN = $(DIR_TOP)/bin

CC = gcc
AR = ar
LD = ld

MKDIR = mkdir -p
RM = rm -rf

CFLAGS  = -g -O3 -fomit-frame-pointer -Wall -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-variable -Wno-attributes -Werror -Wno-strict-overflow  -std=c99
CFLAGS += -DLINUX -DX86_64 -msse4.2
CFLAGS += -I$(DIR_INC) -DIFVC_TRACE=0 -DIFVC_ASSERT=1
CFLAGS += $(ADD_CFLAGS)

LDFLAGS = -L$(DIR_LIB) -lcom -lm -lstdc++
ARFLAGS = r

CSRCS = $(DIR_SRC)/com_img.c \
		$(DIR_SRC)/com_ipred.c \
		$(DIR_SRC)/com_mc.c \
		$(DIR_SRC)/com_recon.c \
		$(DIR_SRC)/com_tbl.c \
		$(DIR_SRC)/com_util.c \
		$(DIR_SRC)/com_itdq.c \
		$(DIR_SRC)/com_df.c \
		$(DIR_SRC)/com_sao.c \
		$(DIR_SRC)/com_ComAdaptiveLoopFilter.c \
		$(DIR_SRC)/com_picman.c \
		$(DIR_SRC)/enc.c \
		$(DIR_SRC)/enc_eco.c \
		$(DIR_SRC)/enc_mode.c \
		$(DIR_SRC)/enc_pinter.c \
		$(DIR_SRC)/enc_pintra.c \
		$(DIR_SRC)/enc_tq.c \
		$(DIR_SRC)/enc_util.c \
		$(DIR_SRC)/enc_tbl.c \
		$(DIR_SRC)/enc_bsw.c \
		$(DIR_SRC)/enc_sad.c \
		$(DIR_SRC)/enc_EncAdaptiveLoopFilter.c \

#		$(DIR_SRC)/dec.c \
#		$(DIR_SRC)/dec_eco.c \
#		$(DIR_SRC)/dec_util.c \
#		$(DIR_SRC)/dec_bsr.c \
#		$(DIR_SRC)/dec_DecAdaptiveLoopFilter.c \

CSRCS_APP =	$(DIR_APP)/app_encoder.c \

#			$(DIR_APP)/app_decoder.c \
#			$(DIR_APP)/app_bitstream_merge.c

OBJS = $(CSRCS:.c=.o)

all: RELEASE

# target for release mode
RELEASE: depend $(OBJS)
	$(MKDIR) $(DIR_LIB)
	$(AR) $(ARFLAGS) $(DIR_LIB)/libcom.a $(OBJS)
	$(MKDIR) $(DIR_BIN)
	$(CC) -o $(DIR_BIN)/app_encoder $(CFLAGS) $(DIR_APP)/app_encoder.c $(LDFLAGS)

#	$(CC) -o $(DIR_BIN)/app_decoder $(CFLAGS) $(DIR_APP)/app_decoder.c $(LDFLAGS)
#	$(CC) -o $(DIR_BIN)/app_bitstream_merge $(CFLAGS) $(DIR_APP)/app_bitstream_merge.c $(LDFLAGS)

depend: Makefile $(CSRCS) $(CSRCS_APP)
	$(CC) -M $(CFLAGS) $(CSRCS) $(CSRCS_APP) > .$@

clean:
	$(RM) $(OBJS) $(DIR_LIB) $(DIR_BIN) .depend

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<

-include .depend

