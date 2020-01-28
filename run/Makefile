

ADD_CFLAGS=-I../../inc/kernel/


test: build run diff

build: 
	make -C ../build/x86_linux/ ADD_CFLAGS="$(ADD_CFLAGS)" 

run: 
	../bin/app_encoder --config ../cfg/encode_IPPP.cfg -v 2 -i akiyo_qcif.yuv -w 176 -h 144 -z 30 -d 8 -f 9 -o akiyo_qcif.enc -r akiyo_qcif.rec

diff:
	@diff akiyo_qcif.enc akiyo_qcif.enc.ref && echo "Success!"

clean: 
	make clean -C ../build/x86_linux/ ADD_CFLAGS="$(ADD_CFLAGS)" 
